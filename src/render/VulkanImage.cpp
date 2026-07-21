#include "render/VulkanImage.h"
#include "render/VulkanBuffer.h"
#include "core/Logger.h"

VulkanSampler::~VulkanSampler()
{
	Clear();
}

void VulkanSampler::Create(const VulkanContext& ctx, const VkSamplerCreateInfo& ci)
{
	device = ctx.GetDevice();
	info = ci;
	VK_RESULT_CHECK(vkCreateSampler(device, &ci, nullptr, &sampler));
}


void VulkanSampler::Clear()
{
	if (device == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE)
		return;
	vkDestroySampler(device, sampler, nullptr);
	sampler = VK_NULL_HANDLE;
}

auto VulkanSampler::GetCreateInfo() -> VkSamplerCreateInfo
{
	VkSamplerCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	ci.magFilter = VkFilter::VK_FILTER_LINEAR;
	ci.minFilter = VkFilter::VK_FILTER_LINEAR;
	ci.mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR;
	ci.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT;
	ci.addressModeV = ci.addressModeU;
	ci.addressModeW = ci.addressModeU;
	ci.mipLodBias = 0.0f;
	ci.maxAnisotropy = 1.0f;
	ci.compareOp = VkCompareOp::VK_COMPARE_OP_NEVER;
	ci.minLod = 0.0f;
	ci.maxLod = 1.0f;
	ci.borderColor = VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	return ci;
}

VulkanImage::VulkanImage(const VulkanContext& ctx, const VkImageCreateInfo& ci, VkImageAspectFlags aspect, VkMemoryPropertyFlags memProp)
{
	Create(ctx, ci, aspect, memProp);
}

VulkanImage::~VulkanImage()
{
	if (ctx == nullptr || img == VK_NULL_HANDLE)
		return;
	const VkDevice device = ctx->GetDevice();
	vkDestroyImageView(device, view, nullptr);
	vkFreeMemory(device, mem, nullptr);
	vkDestroyImage(device, img, nullptr);
}

void VulkanImage::Create(const VulkanContext& ctx, const VkImageCreateInfo& ci, VkImageAspectFlags aspect, VkMemoryPropertyFlags memProp)
{
	this->ctx = &ctx;
	info = ci;
	const VkDevice device = ctx.GetDevice();
	VK_RESULT_CHECK(vkCreateImage(device, &ci, nullptr, &img));

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device, img, &memReqs);
	bufferSize = memReqs.size;

	VkMemoryAllocateInfo memAllocInfo{};
	memAllocInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = ctx.FindMemoryType(memReqs.memoryTypeBits, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_RESULT_CHECK(vkAllocateMemory(device, &memAllocInfo, nullptr, &mem));
	VK_RESULT_CHECK(vkBindImageMemory(device, img, mem, 0));

	VkImageViewCreateInfo viewCi{};
	viewCi.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCi.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
	viewCi.format = info.format;
	viewCi.subresourceRange = { aspect, 0, 1, 0, 1 };
	viewCi.image = img;
	if (ci.imageType == VkImageType::VK_IMAGE_TYPE_3D)
		viewCi.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_3D;
	VK_RESULT_CHECK(vkCreateImageView(device, &viewCi, nullptr, &view));
}

void VulkanImage::SetData(const uint8_t* dataPtr)
{
	if (ctx == nullptr)
		return;
	const VulkanBuffer staging = VulkanBuffer::Create(*ctx,
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		bufferSize, dataPtr);

	// 커맨드 버퍼 할당
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandBufferCount = 1;
	allocInfo.commandPool = ctx->GetCommandPool();
	allocInfo.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	VK_RESULT_CHECK(vkAllocateCommandBuffers(ctx->GetDevice(), &allocInfo, &cmd));

	// 스테이징 -> 버퍼 카피 커맨드 기록
	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = info.extent;

	VkImageMemoryBarrier barrier{};
	barrier.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT;
	barrier.dstAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange = { VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	barrier.image = img;

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);
	vkCmdPipelineBarrier(
		cmd,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier);
	vkCmdCopyBufferToImage(cmd, staging.GetBuffer(), img, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	vkEndCommandBuffer(cmd);

	// 큐에 제출
	VkFence fence = VK_NULL_HANDLE;
	VkFenceCreateInfo fenceCi{};
	fenceCi.sType = VkStructureType::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VK_RESULT_CHECK(vkCreateFence(ctx->GetDevice(), &fenceCi, nullptr, &fence));

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;
	VK_RESULT_CHECK(vkQueueSubmit(ctx->GetGraphicsQueue(), 1, &submitInfo, fence));

	VK_RESULT_CHECK(vkWaitForFences(ctx->GetDevice(), 1, &fence, VK_TRUE, UINT64_MAX));
	vkFreeCommandBuffers(ctx->GetDevice(), ctx->GetCommandPool(), 1, &cmd);
	vkDestroyFence(ctx->GetDevice(), fence, nullptr);
}

auto VulkanImage::GetCreateInfo() -> VkImageCreateInfo
{
	VkImageCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ci.format = VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
	ci.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
	ci.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
	ci.imageType = VkImageType::VK_IMAGE_TYPE_2D;
	ci.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	ci.extent = { 128, 128, 1 };
	ci.mipLevels = 1;
	ci.arrayLayers = 1;
	ci.tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL;
	ci.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
	return ci;
}