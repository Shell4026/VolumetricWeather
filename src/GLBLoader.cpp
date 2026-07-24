#include "GLBLoader.h"
#include "core/Logger.h"
#include "core/Util.h"

#include "tiny_gltf.h"
#include "glm/gtc/type_ptr.hpp"

#include <string>
#include <map>
#include <queue>
#include <cstdint>
auto LoadTextures(const VulkanContext& ctx, const tinygltf::Model& model) -> std::vector<VulkanImage>
{
	std::vector<VulkanImage> imgs;
	for (const tinygltf::Image& img : model.images)
	{
		const uint32_t width = static_cast<uint32_t>(img.width);
		const uint32_t height = static_cast<uint32_t>(img.height);
		VkImageCreateInfo ci = VulkanImage::GetCreateInfo();
		ci.extent = { width, height, 1 };
		ci.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (img.component >= 3 && img.bits == 8) // RGB24
		{
			ci.format = VkFormat::VK_FORMAT_R8G8B8A8_SRGB;
		}
		else
		{
			SH_ERROR_FORMAT("Unsupported format!: ({}, {})", img.component, img.bits);
			continue;
		}
		VulkanImage& vkImage = imgs.emplace_back(ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkImage.SetData(img.image.data());
	}
	return imgs;
}

auto GLBLoader::LoadGLB(const VulkanContext& ctx, const std::filesystem::path& path) -> Model
{
	static tinygltf::TinyGLTF gltfContext;
	tinygltf::Model gltfModel;

	std::vector<uint8_t> binary = util::LoadBinary(path);

	std::string error, warning;
	if (!gltfContext.LoadBinaryFromMemory(&gltfModel, &error, &warning, binary.data(), binary.size()))
	{
		SH_ERROR_FORMAT("{}", error);
		return {};
	}
	if (gltfModel.scenes.empty())
	{
		SH_ERROR_FORMAT("Scene is empty: {}", path.string());
		return {};
	}
	Model resultModel{};
	resultModel.textures = LoadTextures(ctx, gltfModel);
	std::vector<Node>& nodes = resultModel.nodes;

	const tinygltf::Scene& scene = gltfModel.scenes[0];
	nodes.reserve(gltfModel.nodes.size());
	Node& rootNode = nodes.emplace_back();
	rootNode.name = "root";

	using GLTFNodeIdx = int;
	using NodeIdx = int;
	std::map<GLTFNodeIdx, NodeIdx> nodeMap;

	std::queue<std::pair<GLTFNodeIdx, NodeIdx>> nodeQ{}; // 노드idx, 부모idx
	for (int nodeIdx : scene.nodes)
		nodeQ.push({ nodeIdx, 0 });

	while (!nodeQ.empty())
	{
		auto [gltfNodeIdx, parentNodeIdx] = nodeQ.front();
		nodeQ.pop();

		const int idx = nodes.size();
		nodeMap.insert({ gltfNodeIdx, idx });

		nodes[parentNodeIdx].childrenIdxs.push_back(idx);

		const tinygltf::Node& gltfNode = gltfModel.nodes[gltfNodeIdx];

		for (int childIdx : gltfNode.children)
			nodeQ.push({ childIdx, idx });

		Node& node = nodes.emplace_back();
		node.name = gltfNode.name;

		glm::mat4 matrix{ 1.0f };
		if (gltfNode.matrix.size() == 16)
			matrix = glm::make_mat4x4(gltfNode.matrix.data());
		else
		{
			if (gltfNode.translation.size() == 3)
				matrix = glm::translate(matrix, glm::vec3{ glm::make_vec3(gltfNode.translation.data()) });
			if (gltfNode.rotation.size() == 4)
			{
				glm::quat q{ glm::make_quat(gltfNode.rotation.data()) };
				matrix *= glm::mat4(q);
			}
			if (gltfNode.scale.size() == 3)
				matrix = glm::scale(matrix, glm::vec3{ glm::make_vec3(gltfNode.scale.data()) });
		}
		node.modelMatrix = matrix;

		if (gltfNode.mesh < 0)
			continue;

		std::vector<GLBVertex> verts;
		std::vector<uint32_t> indices;
		std::vector<SubMesh> subMeshes;

		const tinygltf::Mesh& gltfMesh = gltfModel.meshes[gltfNode.mesh];

		for (const tinygltf::Primitive& primitive : gltfMesh.primitives)
		{
			SubMesh& subMesh = subMeshes.emplace_back();
			uint32_t firstIndex = static_cast<uint32_t>(indices.size());
			uint32_t vertexStart = static_cast<uint32_t>(verts.size());

			const float* positionBuffer = nullptr;
			const float* normalsBuffer = nullptr;
			const float* texCoordsBuffer = nullptr;
			std::size_t vertexCount = 0;

			if (auto it = primitive.attributes.find("POSITION"); it != primitive.attributes.end())
			{
				const tinygltf::Accessor& accessor = gltfModel.accessors[it->second];
				const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
				positionBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				vertexCount = accessor.count;
			}
			if (auto it = primitive.attributes.find("NORMAL"); it != primitive.attributes.end())
			{
				const tinygltf::Accessor& accessor = gltfModel.accessors[it->second];
				const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
				normalsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
			}
			if (auto it = primitive.attributes.find("TEXCOORD_0"); it != primitive.attributes.end())
			{
				const tinygltf::Accessor& accessor = gltfModel.accessors[it->second];
				const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
				texCoordsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
			}

			for (std::size_t v = 0; v < vertexCount; v++)
			{
				GLBVertex& vert = verts.emplace_back();
				vert.pos = glm::make_vec3(&positionBuffer[v * 3]);
				vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
				vert.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec2(0.0f);
			}
			// 인덱스
			{
				const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.indices];
				const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];
				subMesh.indexOffset = indices.size();
				subMesh.indexCount = static_cast<uint32_t>(accessor.count);
				switch (accessor.componentType)
				{
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
				{
					const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++)
						indices.push_back(buf[index] + vertexStart);
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
				{
					const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++)
						indices.push_back(buf[index] + vertexStart);
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
				{
					const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++)
						indices.push_back(buf[index] + vertexStart);
					break;
				}
				default:
					SH_ERROR_FORMAT("Index component type {} not supported!", accessor.componentType);
					return {};
				}
			}
			if (primitive.material != -1)
				node.textureIdx = gltfModel.materials[primitive.material].pbrMetallicRoughness.baseColorTexture.index;
			node.meshPtr = std::make_unique<Mesh<GLBVertex>>();
			node.meshPtr->SetVertices(std::move(verts));
			node.meshPtr->SetIndices(std::move(indices));
			node.meshPtr->CreateBuffers(ctx);
		}
	}
	return resultModel;
}
