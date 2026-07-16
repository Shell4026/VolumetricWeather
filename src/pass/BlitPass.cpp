#include "pass/BlitPass.h"

#include "core/Logger.h"

#include "render/VulkanImage.h"
#include "render/VulkanBuffer.h"

#include <glm/gtc/packing.hpp>
#include <vulkan/utility/vk_format_utils.h>
BlitPass::BlitPass() = default;
BlitPass::~BlitPass()
{
	bStopThread = true;
	bHasRequest.store(true, std::memory_order::release);
	bHasRequest.notify_all();

	thr.join();
}

void BlitPass::Clear()
{
	if (timeline != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(ctx->GetDevice(), timeline, nullptr);
		timeline = VK_NULL_HANDLE;
	}
	APass::Clear();
}

void BlitPass::Record(const VulkanContext& ctx, const FrameContext& frame)
{
	if (buffer == nullptr)
		CreateBuffer();

	std::lock_guard<std::mutex> lock{ mu };
	if (blitQueue.empty())
	{
		ts.signalSemaphoreValueCount = 0;
		submitInfo.signalSemaphoreCount = 0;
		return;
	}
	ts.signalSemaphoreValueCount = 1;
	submitInfo.signalSemaphoreCount = 1;
	const VkCommandBuffer cmd = GetCommandBuffer();

	std::size_t offset = 0;
	for (BlitRequest& req : blitQueue)
	{
		VkImageAspectFlags aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
		const VkFormat format = req.img->GetInfo().format;
		if (format == VkFormat::VK_FORMAT_D32_SFLOAT || format == VkFormat::VK_FORMAT_D24_UNORM_S8_UINT)
			aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;

		VkBufferImageCopy cpy{};
		cpy.bufferOffset = offset;
		cpy.bufferRowLength = 0;
		cpy.bufferImageHeight = 0;
		cpy.imageSubresource.aspectMask = aspect;
		cpy.imageSubresource.mipLevel = 0;
		cpy.imageSubresource.baseArrayLayer = 0;
		cpy.imageSubresource.layerCount = 1;
		cpy.imageOffset = VkOffset3D{ req.x, req.y, 0 };
		cpy.imageExtent = { 1, 1, 1 };

		vkCmdCopyImageToBuffer(cmd, req.img->GetImage(), VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer->GetBuffer(), 1, &cpy);

		offset += vkuFormatTexelBlockSize(format);

		req.requestTimelineValue = timelineValue;
	}
	++timelineValue;

	bHasRequest.store(true, std::memory_order::release);
	bHasRequest.notify_all();
}

void BlitPass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	APass::SetUsages(ctx, frame);
	for (BlitRequest& req : blitQueue)
	{
		const VkFormat format = req.img->GetInfo().format;
		VkImageAspectFlags aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
		if (format == VkFormat::VK_FORMAT_D32_SFLOAT || format == VkFormat::VK_FORMAT_D24_UNORM_S8_UINT)
			aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;

		AddUsage(req.img->GetImage(), aspect, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	}
}

auto BlitPass::RequestBlit(const VulkanImage& img, int32_t x, int32_t y) -> std::future<glm::vec4>
{
	BlitRequest req;
	req.img = &img;
	req.x = x;
	req.y = y;

	std::future<glm::vec4> future;
	future = req.promise.get_future();

	std::lock_guard<std::mutex> lock{ mu };
	blitQueue.push_back(std::move(req));
	return future;
}

void BlitPass::PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout)
{
	VkSemaphoreTypeCreateInfo typeCI{};
	typeCI.sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	typeCI.initialValue = 0;
	typeCI.semaphoreType = VkSemaphoreType::VK_SEMAPHORE_TYPE_TIMELINE;

	VkSemaphoreCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	ci.pNext = &typeCI;
	VK_RESULT_CHECK(vkCreateSemaphore(ctx.GetDevice(), &ci, nullptr, &timeline));

	ts.sType = VkStructureType::VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
	ts.signalSemaphoreValueCount = 1;
	ts.pSignalSemaphoreValues = &timelineValue;

	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &timeline;
	submitInfo.pNext = &ts;

	thr = std::thread(
		[this]()
		{
			const VkDevice device = this->ctx->GetDevice();
			const VkFence fence = GetFence();
			while (!bStopThread)
			{
				bHasRequest.wait(false, std::memory_order::acquire);
				if (bStopThread)
					return;

				std::unique_lock<std::mutex> lock{ mu };
				bHasRequest.store(false, std::memory_order_release);
				if (blitQueue.empty())
					continue;

				std::vector<BlitRequest> reqs = std::move(blitQueue);
				blitQueue.clear();
				lock.unlock();

				std::size_t offset = 0;
				for (BlitRequest& req : reqs)
				{
					uint64_t waitValue = req.requestTimelineValue + 1;
					VkSemaphoreWaitInfo waitInfo{};
					waitInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
					waitInfo.semaphoreCount = 1;
					waitInfo.pSemaphores = &timeline;
					waitInfo.pValues = &waitValue;
					vkWaitSemaphores(device, &waitInfo, UINT64_MAX);

					const std::size_t texelSize = vkuFormatTexelBlockSize(req.img->GetInfo().format);

					glm::vec4 result{};
					buffer->Map(VK_WHOLE_SIZE, offset);
					if (texelSize == 8)
					{
						const uint16_t* const data = reinterpret_cast<uint16_t*>(buffer->GetData());
						result.r = glm::unpackHalf1x16(data[0]);
						result.g = glm::unpackHalf1x16(data[1]);
						result.b = glm::unpackHalf1x16(data[2]);
						result.a = glm::unpackHalf1x16(data[3]);
					}
					else if (texelSize == 4)
					{
						const uint8_t* const data = reinterpret_cast<uint8_t*>(buffer->GetData());
						result.r = data[0];
						result.g = data[1];
						result.b = data[2];
						result.a = data[3];
					}
					buffer->UnMap();
					req.promise.set_value(result);
					offset += vkuFormatTexelBlockSize(req.img->GetInfo().format);
				}
			}
		}
	);
}

void BlitPass::CreateBuffer()
{
	buffer = std::make_unique<VulkanBuffer>(VulkanBuffer::Create(
		*ctx,
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		sizeof(uint8_t) * 4 * CAPACITY
	));
}
