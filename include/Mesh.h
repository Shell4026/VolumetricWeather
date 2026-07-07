#pragma once
#include "VulkanContext.h"
#include "VulkanBuffer.h"
#include "core/Logger.h"

#include <glm/glm.hpp>

#include <vector>
#include <type_traits>
#include <cstdint>
#include <memory>

template<typename T>
concept Vertex = std::is_class_v<T> && std::is_trivial_v<T>;

struct SubMesh
{
	std::size_t indexOffset = 0;
	std::size_t indexCount = 0;
};

template<Vertex T>
class Mesh
{
public:
	Mesh() = default;
	Mesh(std::vector<T> verts, std::vector<uint32_t> indices) :
		verts(std::move(verts)), indices(std::move(indices))
	{
	}
	~Mesh()
	{
		Clear();
	}
	void Init(std::vector<T> verts, std::vector<uint32_t> indices);
	void CreateBuffer(const VulkanContext& ctx);
	void Clear();

	auto GetVertexBuffer() const -> VulkanBuffer* { return buffer.get(); }
	auto GetIndexBuffer() const -> VulkanBuffer* { return indexBuffer.get(); }
	auto GetVerts() const -> const std::vector<T>& { return verts; }
	auto GetIndices() const -> const std::vector<uint32_t>& { return indices; }
public:
	using Type = T;
private:
	std::unique_ptr<VulkanBuffer> buffer;
	std::unique_ptr<VulkanBuffer> indexBuffer;

	std::vector<T> verts;
	std::vector<uint32_t> indices;
};

template<Vertex T>
inline void Mesh<T>::Init(std::vector<T> verts, std::vector<uint32_t> indices)
{
	this->verts = std::move(verts);
	this->indices = std::move(indices);
}

template<Vertex T>
inline void Mesh<T>::CreateBuffer(const VulkanContext& ctx)
{
	// 스테이징 버퍼
	VkMemoryPropertyFlags memProp = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	VulkanBuffer vertStaging = VulkanBuffer::Create(ctx, VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT, memProp, sizeof(T) * verts.size(), verts.data());
	VulkanBuffer indexStaging = VulkanBuffer::Create(ctx, VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT, memProp, sizeof(uint32_t) * indices.size(), indices.data());

	// 실제 버퍼
	memProp = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	VkBufferUsageFlags usage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	buffer = std::make_unique<VulkanBuffer>(VulkanBuffer::Create(ctx, usage, memProp, sizeof(T) * verts.size()));
	usage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	indexBuffer = std::make_unique<VulkanBuffer>(VulkanBuffer::Create(ctx, usage, memProp, sizeof(T) * verts.size()));

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
	vkCmdCopyBuffer(cmd, vertStaging.GetBuffer(), buffer->GetBuffer(), 1, &cpy);
	cpy.size = sizeof(uint32_t) * indices.size();
	vkCmdCopyBuffer(cmd, indexStaging.GetBuffer(), indexBuffer->GetBuffer(), 1, &cpy);
	vkEndCommandBuffer(cmd);

	// 큐에 제출
	VkFence fence = VK_NULL_HANDLE;
	VkFenceCreateInfo fenceCi{};
	fenceCi.sType = VkStructureType::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	vkCreateFence(ctx.GetDevice(), &fenceCi, nullptr, &fence);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;
	VK_RESULT_CHECK(vkQueueSubmit(ctx.GetGraphicsQueue(), 1, &submitInfo, fence));

	vkWaitForFences(ctx.GetDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
	vkFreeCommandBuffers(ctx.GetDevice(), ctx.GetCommandPool(), 1, &cmd);
	vkDestroyFence(ctx.GetDevice(), fence, nullptr);
}

template<Vertex T>
inline void Mesh<T>::Clear()
{
	buffer.reset();
	indexBuffer.reset();
	verts.clear();
	indices.clear();
}