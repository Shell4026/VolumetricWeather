#pragma once
#include "VulkanContext.h"
#include "VulkanBuffer.h"
#include "core/Logger.h"

#include <glm/glm.hpp>

#include <vector>
#include <type_traits>
#include <cstdint>
#include <memory>
#include <concepts>
#include <array>
template<typename T>
concept Vertex = std::is_class_v<T> && std::is_trivial_v<T> && requires(T)
{
	requires std::same_as<decltype(T::pos), glm::vec3>;
	{ T::GetVertexInputAttributeDescription() } -> std::same_as<const std::vector<VkVertexInputAttributeDescription>&>;
	{ T::GetVertexInputBindingDescription() } -> std::same_as<VkVertexInputBindingDescription>;
};

struct SubMesh
{
	std::size_t indexOffset = 0;
	std::size_t indexCount = 0;
};

class AMeshBase
{
public:
	struct Face
	{
		std::array<uint32_t, 3> verts;
	};
public:
	AMeshBase() = default;
	AMeshBase(AMeshBase&& other) noexcept:
		buffer(std::move(other.buffer)),
		indexBuffer(std::move(other.indexBuffer)),
		indices(std::move(other.indices)),
		faces(std::move(other.faces))
	{}
	virtual ~AMeshBase() = default;

	void SetIndices(std::vector<uint32_t> indices);
	void ClearIndices() { indices.clear(); }
	void CreateBuffers(const VulkanContext& ctx);
	void ClearBuffers();

	auto GetVertexBuffer() const -> VulkanBuffer* { return buffer.get(); }
	auto GetIndexBuffer() const -> VulkanBuffer* { return indexBuffer.get(); }
	auto GetIndices() const -> const std::vector<uint32_t>& { return indices; }
	auto GetFaces() const -> const std::vector<Face>& { return faces; }
protected:
	virtual auto CreateVertexBuffer(const VulkanContext& ctx) -> VulkanBuffer = 0;
private:
	auto CreateIndexBuffer(const VulkanContext& ctx) -> VulkanBuffer;
private:
	std::unique_ptr<VulkanBuffer> buffer;
	std::unique_ptr<VulkanBuffer> indexBuffer;
	std::vector<uint32_t> indices;
	std::vector<Face> faces;
};

template<Vertex T>
class Mesh : public AMeshBase
{
public:
	Mesh() = default;
	Mesh(Mesh&& other) noexcept :
		AMeshBase(std::move(other)),
		verts(std::move(other.verts))
	{}
	Mesh(std::vector<T> verts, std::vector<uint32_t> indices) :
		verts(std::move(verts)), indices(std::move(indices))
	{}
	~Mesh()
	{
		Clear();
	}
	void SetVertices(std::vector<T> verts) { this->verts = std::move(verts); }
	void Clear();

	auto GetVertices() const -> const std::vector<T>& { return verts; }
protected:
	auto CreateVertexBuffer(const VulkanContext& ctx) -> VulkanBuffer override;
public:
	using Type = T;
private:
	std::vector<T> verts;
};

template<Vertex T>
inline auto Mesh<T>::CreateVertexBuffer(const VulkanContext& ctx) -> VulkanBuffer
{
	// 스테이징 버퍼
	VkMemoryPropertyFlags memProp = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	VulkanBuffer vertStaging = VulkanBuffer::Create(ctx, VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT, memProp, sizeof(T) * verts.size(), verts.data());

	// 실제 버퍼
	memProp = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	const VkBufferUsageFlags usage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	VulkanBuffer buffer{ VulkanBuffer::Create(ctx, usage, memProp, sizeof(T) * verts.size()) };

	// 커맨드 버퍼 할당
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandBufferCount = 1;
	allocInfo.commandPool = ctx.GetCommandPool();
	allocInfo.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	VK_RESULT_CHECK(vkAllocateCommandBuffers(ctx.GetDevice(), &allocInfo, &cmd));

	// 스테이징 -> 버퍼 카피 커맨드 기록
	VkBufferCopy cpy{};
	cpy.size = sizeof(T) * verts.size();
	cpy.dstOffset = 0;
	cpy.dstOffset = 0;
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);
	vkCmdCopyBuffer(cmd, vertStaging.GetBuffer(), buffer.GetBuffer(), 1, &cpy);
	vkEndCommandBuffer(cmd);

	// 큐에 제출
	VkFence fence = VK_NULL_HANDLE;
	VkFenceCreateInfo fenceCi{};
	fenceCi.sType = VkStructureType::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VK_RESULT_CHECK(vkCreateFence(ctx.GetDevice(), &fenceCi, nullptr, &fence));

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;
	VK_RESULT_CHECK(vkQueueSubmit(ctx.GetGraphicsQueue(), 1, &submitInfo, fence));

	VK_RESULT_CHECK(vkWaitForFences(ctx.GetDevice(), 1, &fence, VK_TRUE, UINT64_MAX));
	vkFreeCommandBuffers(ctx.GetDevice(), ctx.GetCommandPool(), 1, &cmd);
	vkDestroyFence(ctx.GetDevice(), fence, nullptr);
	return buffer;
}

template<Vertex T>
inline void Mesh<T>::Clear()
{
	ClearBuffers();
	verts.clear();
	ClearIndices();
}