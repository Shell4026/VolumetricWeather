#include "render/Mesh.h"

void AMeshBase::CreateBuffers(const VulkanContext& ctx)
{
	buffer = std::make_unique<VulkanBuffer>(CreateVertexBuffer(ctx));
	indexBuffer = std::make_unique<VulkanBuffer>(CreateIndexBuffer(ctx));
}

void AMeshBase::ClearBuffers()
{
	buffer.reset();
	indexBuffer.reset();
}

auto AMeshBase::CreateIndexBuffer(const VulkanContext& ctx) -> VulkanBuffer
{
	// 스테이징 버퍼
	VkMemoryPropertyFlags memProp = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	VulkanBuffer indexStaging = VulkanBuffer::Create(ctx, VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT, memProp, sizeof(uint32_t) * indices.size(), indices.data());

	// 실제 버퍼
	memProp = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	const VkBufferUsageFlags usage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	VulkanBuffer buffer{ VulkanBuffer::Create(ctx, usage, memProp, sizeof(uint32_t) * indices.size()) };

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
	cpy.size = sizeof(uint32_t) * indices.size();
	cpy.dstOffset = 0;
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);
	vkCmdCopyBuffer(cmd, indexStaging.GetBuffer(), buffer.GetBuffer(), 1, &cpy);
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