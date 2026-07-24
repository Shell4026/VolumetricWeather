#pragma once
#include "core/Util.hpp"

#include "render/Mesh.h"

#include "glm/glm.hpp"

#include <unordered_map>
class VulkanContext;
struct VolumeVertex
{
	glm::vec3 pos;

	static auto GetVertexInputAttributeDescription() -> const std::vector<VkVertexInputAttributeDescription>&
	{
		static std::vector<VkVertexInputAttributeDescription> attribs;
		if (attribs.empty())
		{
			VkVertexInputAttributeDescription& vertexInputAttrib0 = attribs.emplace_back();
			vertexInputAttrib0.binding = 0;
			vertexInputAttrib0.location = 0;
			vertexInputAttrib0.format = VkFormat::VK_FORMAT_R32G32B32_SFLOAT;
			vertexInputAttrib0.offset = offsetof(VolumeVertex, pos);
		}
		return attribs;
	}
	static auto GetVertexInputBindingDescription() -> VkVertexInputBindingDescription
	{
		static VkVertexInputBindingDescription vertexInputBinding{};
		vertexInputBinding.binding = 0;
		vertexInputBinding.stride = sizeof(VolumeVertex);
		vertexInputBinding.inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;
		return vertexInputBinding;
	}
};

class FakeVolumeGenerator
{
public:
	template<Vertex T>
	static auto GenerateShadowVolumeMesh(const VulkanContext& ctx, const Mesh<T>& mesh, const glm::vec3& sunDir, const glm::mat4& modelMatrix, float length = 32'000.f) -> Mesh<VolumeVertex>;
private:
	struct VertexKey
	{
		int64_t x;
		int64_t y;
		int64_t z;
		auto operator==(const VertexKey&) const -> bool = default;
	};
	static auto CreateVertexKey(const glm::vec3& pos) -> VertexKey
	{
		constexpr double scale = 10'000;
		VertexKey key{};
		key.x = std::llround(static_cast<double>(pos.x) * scale);
		key.y = std::llround(static_cast<double>(pos.y) * scale);
		key.z = std::llround(static_cast<double>(pos.z) * scale);
		return key;
	}
	struct VertexKeyHasher
	{
		auto operator()(const VertexKey& key) const noexcept -> std::size_t
		{
			std::size_t hash = std::hash<int64_t>{}(key.x);
			hash = util::CombineHash(hash, std::hash<int64_t>{}(key.y));
			hash = util::CombineHash(hash, std::hash<int64_t>{}(key.z));
			return hash;
		}
	};

	struct Edge
	{
		uint32_t vid0 = 0xFFFFFFFF;
		uint32_t vid1 = 0xFFFFFFFF;
		auto operator==(const Edge&) const -> bool = default;
		static auto Create(uint32_t a, uint32_t b) -> Edge
		{
			if (a > b)
				std::swap(a, b);

			return Edge{
				.vid0 = a,
				.vid1 = b,
			};
		}
	};
	struct EdgeHasher
	{
		auto operator()(const Edge& edge) const noexcept -> std::size_t
		{
			std::size_t hash = std::hash<int64_t>{}(edge.vid0);
			hash = util::CombineHash(hash, std::hash<int64_t>{}(edge.vid1));
			return hash;
		}
	};
};

template<Vertex T>
inline auto FakeVolumeGenerator::GenerateShadowVolumeMesh(const VulkanContext& ctx, const Mesh<T>& mesh, const glm::vec3& sunDir, const glm::mat4& modelMatrix, float length) -> Mesh<VolumeVertex>
	{
	glm::vec3 localSunDir = glm::normalize(glm::vec3(glm::inverse(modelMatrix) * glm::vec4(sunDir, 0.f)));
	Mesh<VolumeVertex> volumeMesh{};

	const glm::vec3& extrusionDirection = localSunDir;
	const glm::vec3 extrusionOffset = localSunDir * length;

	const std::vector<T>& sourceVertices = mesh.GetVertices();
	std::unordered_map<VertexKey, uint32_t, VertexKeyHasher> topologyIdMap;
	std::vector<glm::vec3> topologyPositions;
	topologyIdMap.reserve(sourceVertices.size());
	topologyPositions.reserve(sourceVertices.size());

	auto getTopologyId =
		[&](const glm::vec3& position) -> uint32_t
		{
			const VertexKey key = CreateVertexKey(position);

			if (const auto it = topologyIdMap.find(key);
				it != topologyIdMap.end())
			{
				return it->second;
			}

			const uint32_t topologyId =
				static_cast<uint32_t>(topologyPositions.size());

			topologyPositions.push_back(position);
			topologyIdMap.emplace(key, topologyId);

			return topologyId;
		};

	struct SelectedFace
	{
		uint32_t v0;
		uint32_t v1;
		uint32_t v2;
	};
	std::vector<SelectedFace> selectedFaces;

	struct EdgeInfo
	{
		uint32_t from = 0;
		uint32_t to = 0;

		std::vector<uint32_t> faceIndices;
	};

	std::unordered_map<Edge, EdgeInfo, EdgeHasher> edgeMap;

	selectedFaces.reserve(mesh.GetFaces().size());
	edgeMap.reserve(mesh.GetFaces().size() * 3);

	auto addEdge =
		[&](uint32_t from, uint32_t to, uint32_t selectedFaceIndex)
		{
			const Edge key = Edge::Create(from, to);

			auto [it, inserted] = edgeMap.try_emplace(key);

			EdgeInfo& info = it->second;

			if (inserted)
			{
				// 무방향 key와 별개로 원래 방향을 보관한다.
				info.from = from;
				info.to = to;
			}
			info.faceIndices.push_back(selectedFaceIndex);
		};

	for (const AMeshBase::Face& face : mesh.GetFaces())
	{
		const uint32_t sourceV0 = face.verts[0];
		const uint32_t sourceV1 = face.verts[1];
		const uint32_t sourceV2 = face.verts[2];

		const glm::vec3& p0 = sourceVertices[sourceV0].pos;
		const glm::vec3& p1 = sourceVertices[sourceV1].pos;
		const glm::vec3& p2 = sourceVertices[sourceV2].pos;

		const glm::vec3 normal = glm::normalize(glm::cross(p2 - p0, p1 - p0));

		if (glm::dot(normal, extrusionDirection) >= 0.0f)
			continue;

		const uint32_t v0 = getTopologyId(p0);
		const uint32_t v1 = getTopologyId(p1);
		const uint32_t v2 = getTopologyId(p2);

		if (v0 == v1 || v1 == v2 || v2 == v0)
			continue;

		const uint32_t selectedFaceIndex =
			static_cast<uint32_t>(selectedFaces.size());

		selectedFaces.push_back(
			SelectedFace
			{
				.v0 = v0,
				.v1 = v1,
				.v2 = v2,
			}
			);
		addEdge(v0, v1, selectedFaceIndex);
		addEdge(v1, v2, selectedFaceIndex);
		addEdge(v2, v0, selectedFaceIndex);
	}
	if (selectedFaces.empty())
		return volumeMesh;

	std::vector<VolumeVertex> volumeVertices;
	volumeVertices.reserve(topologyPositions.size() * 2);

	std::vector<uint32_t> frontVertexIndices(topologyPositions.size());
	std::vector<uint32_t> extrudedVertexIndices(topologyPositions.size());

	auto appendVertex =
		[&](const glm::vec3& position) -> uint32_t
		{
			const uint32_t index =
				static_cast<uint32_t>(volumeVertices.size());

			VolumeVertex vertex{};
			vertex.pos = position;

			volumeVertices.push_back(vertex);

			return index;
		};

	for (uint32_t topologyId = 0; topologyId < topologyPositions.size(); ++topologyId)
	{
		const glm::vec3& position = topologyPositions[topologyId];

		frontVertexIndices[topologyId] = appendVertex(position);
		extrudedVertexIndices[topologyId] = appendVertex(position + extrusionOffset);
	}

	std::vector<uint32_t> indices;
	indices.reserve(selectedFaces.size() * 6 + edgeMap.size() * 6);

	auto appendTriangle =
		[&](uint32_t i0, uint32_t i1, uint32_t i2)
		{
			indices.push_back(i0);
			indices.push_back(i1);
			indices.push_back(i2);
		};

	for (const SelectedFace& face : selectedFaces)
	{
		const uint32_t front0 = frontVertexIndices[face.v0];
		const uint32_t front1 = frontVertexIndices[face.v1];
		const uint32_t front2 = frontVertexIndices[face.v2];

		const uint32_t extruded0 = extrudedVertexIndices[face.v0];
		const uint32_t extruded1 = extrudedVertexIndices[face.v1];
		const uint32_t extruded2 = extrudedVertexIndices[face.v2];

		appendTriangle(front0, front1, front2);
		appendTriangle(extruded0, extruded2, extruded1);
	}

	// 선택된 면 집합에서 정확히 한 번만 등장한 엣지가 외곽 boundary edge다.
	for (const auto& [edge, info] : edgeMap)
	{
		if (info.faceIndices.size() != 1)
			continue;

		const uint32_t front0 = frontVertexIndices[info.from];
		const uint32_t front1 = frontVertexIndices[info.to];

		const uint32_t extruded0 = extrudedVertexIndices[info.from];
		const uint32_t extruded1 = extrudedVertexIndices[info.to];

		appendTriangle(front0, extruded0, extruded1);
		appendTriangle(front0, extruded1, front1);
	}

	volumeMesh.SetVertices(std::move(volumeVertices));
	volumeMesh.SetIndices(std::move(indices));
	volumeMesh.CreateBuffers(ctx);
	return volumeMesh;
}