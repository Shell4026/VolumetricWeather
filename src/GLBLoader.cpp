#include "GLBLoader.h"
#include "core/Logger.h"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"
#include "glm/gtc/type_ptr.hpp"

#include <fstream>
#include <string>
#include <map>
#include <queue>
auto GLBLoader::LoadGLB(const VulkanContext& ctx, const std::filesystem::path& path) -> std::vector<Node>
{
	static tinygltf::TinyGLTF gltfContext;
	tinygltf::Model gltfModel;
	std::ifstream file{ path, std::ios::ate | std::ios::binary };
	if (!file.is_open())
	{
		SH_ERROR_FORMAT("Failed to read glb: {}", path.string());
		return {};
	}
	const std::size_t fileSize = (size_t)file.tellg();
	std::vector<uint8_t> binary(fileSize);
	file.seekg(0);
	file.read(reinterpret_cast<char*>(binary.data()), fileSize);
	file.close();

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
	const tinygltf::Scene& scene = gltfModel.scenes[0];
	std::vector<Node> nodes;
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

		std::vector<Vertex> verts;
		std::vector<uint32_t> indices;
		std::vector<SubMesh> subMeshes;

		const tinygltf::Mesh& gltfMesh = gltfModel.meshes[gltfNode.mesh];

		for (auto& primitive : gltfMesh.primitives)
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
				Vertex& vert = verts.emplace_back();
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
			node.meshPtr = std::make_unique<Mesh<Vertex>>();
			node.meshPtr->Init(std::move(verts), std::move(indices));
			node.meshPtr->CreateBuffer(ctx);
		}
	}
	return nodes;
}
