#include "pass/BlitPass.h"

#include "core/Logger.h"

#include "render/VulkanImage.h"
#include "render/VulkanBuffer.h"

#include <glm/gtc/packing.hpp>
#include <vulkan/utility/vk_format_utils.h>
#include <limits>
#include <stdexcept>

BlitPass::BlitPass() = default;
BlitPass::~BlitPass()
{
	Clear();
}

void BlitPass::Clear()
{
	bStopThread.store(true, std::memory_order::release);
	bHasRequest.store(true, std::memory_order::release);
	bHasRequest.notify_all();
	if (thr.joinable())
		thr.join();

	{
		std::lock_guard<std::mutex> lock{ mu };
		pendingRequests.clear();
		submittedRequests.clear();
	}

	if (timeline != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(ctx->GetDevice(), timeline, nullptr);
		timeline = VK_NULL_HANDLE;
	}
	APass::Clear();
}

void BlitPass::Record(const VulkanContext& ctx, const FrameContext& frame)
{
	std::lock_guard<std::mutex> lock{ mu };
	if (pendingRequests.empty())
	{
		ts.signalSemaphoreValueCount = 0;
		submitInfo.signalSemaphoreCount = 0;
		return;
	}
	ts.signalSemaphoreValueCount = 1;
	submitInfo.signalSemaphoreCount = 1;
	const VkCommandBuffer cmd = GetCommandBuffer();
	const uint64_t completionValue = ++timelineValue;

	for (BlitRequest& req : pendingRequests)
	{
		VkImageAspectFlags aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
		const VkFormat format = req.format;
		if (format == VkFormat::VK_FORMAT_D32_SFLOAT || format == VkFormat::VK_FORMAT_D24_UNORM_S8_UINT)
			aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;

		VkBufferImageCopy cpy{};
		cpy.bufferOffset = 0;
		cpy.bufferRowLength = 0;
		cpy.bufferImageHeight = 0;
		cpy.imageSubresource.aspectMask = aspect;
		cpy.imageSubresource.mipLevel = 0;
		cpy.imageSubresource.baseArrayLayer = 0;
		cpy.imageSubresource.layerCount = 1;
		cpy.imageOffset = VkOffset3D{ req.x, req.y, 0 };
		cpy.imageExtent = { req.width, req.height, 1 };

		vkCmdCopyImageToBuffer(cmd, req.image, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, req.buffer->GetBuffer(), 1, &cpy);
		req.completionValue = completionValue;
	}

	for (BlitRequest& req : pendingRequests)
		submittedRequests.push_back(std::move(req));
	pendingRequests.clear();

	bHasRequest.store(true, std::memory_order::release);
	bHasRequest.notify_all();
}

void BlitPass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	APass::SetUsages(ctx, frame);
	std::lock_guard<std::mutex> lock{ mu };
	for (BlitRequest& req : pendingRequests)
	{
		const VkFormat format = req.format;
		VkImageAspectFlags aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
		if (format == VkFormat::VK_FORMAT_D32_SFLOAT || format == VkFormat::VK_FORMAT_D24_UNORM_S8_UINT)
			aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;

		AddUsage(req.image, aspect, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	}
}

auto BlitPass::RequestBlit(const VulkanImage& img, int32_t x, int32_t y, uint32_t width, uint32_t height) -> std::future<std::vector<glm::vec4>>
{
	const std::size_t texelSize = vkuFormatTexelBlockSize(img.GetInfo().format);

	BlitRequest req;
	req.image = img.GetImage();
	req.format = img.GetInfo().format;
	req.buffer = CreateBuffer(width * height * texelSize);
	req.x = x;
	req.y = y;
	req.width = width;
	req.height = height;

	std::future<std::vector<glm::vec4>> future;
	future = req.promise.get_future();

	std::lock_guard<std::mutex> lock{ mu };
	pendingRequests.push_back(std::move(req));
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

	bStopThread.store(false, std::memory_order::release);
	bHasRequest.store(false, std::memory_order::release);
	thr = std::thread(
		[this]()
		{
			const VkDevice device = this->ctx->GetDevice();
			while (!bStopThread.load(std::memory_order::acquire))
			{
				bHasRequest.wait(false, std::memory_order::acquire);
				if (bStopThread.load(std::memory_order::acquire))
					return;

				std::unique_lock<std::mutex> lock{ mu };
				bHasRequest.store(false, std::memory_order_release);
				if (submittedRequests.empty())
					continue;

				std::vector<BlitRequest> reqs = std::move(submittedRequests);
				submittedRequests.clear();
				lock.unlock();

				for (BlitRequest& req : reqs)
				{
					uint64_t waitValue = req.completionValue;
					VkSemaphoreWaitInfo waitInfo{};
					waitInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
					waitInfo.semaphoreCount = 1;
					waitInfo.pSemaphores = &timeline;
					waitInfo.pValues = &waitValue;
					VkResult waitResult = VkResult::VK_TIMEOUT;
					while (!bStopThread.load(std::memory_order::acquire) && waitResult == VkResult::VK_TIMEOUT)
						waitResult = vkWaitSemaphores(device, &waitInfo, 10'000'000);
					if (bStopThread.load(std::memory_order::acquire))
						return;

					const std::size_t texelSize = vkuFormatTexelBlockSize(req.format);
					const std::size_t texelCount = req.width * req.height;
					std::vector<glm::vec4> result(texelCount);
					req.buffer->Map();
					const auto* data = static_cast<const uint8_t*>(req.buffer->GetData());
					for (std::size_t i = 0; i < texelCount; ++i)
						result[i] = DecodeTexel(data + i * texelSize, req.format);
					req.buffer->UnMap();
					req.promise.set_value(std::move(result));
				}
			}
		}
	);
}

auto BlitPass::CreateBuffer(std::size_t size) const -> std::unique_ptr<VulkanBuffer>
{
	return std::make_unique<VulkanBuffer>(VulkanBuffer::Create(
		*ctx,
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		size
	));
}

auto BlitPass::DecodeTexel(const uint8_t* data, VkFormat format) -> glm::vec4
{
	switch (format)
	{
	case VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT:
	{
		const auto* values = reinterpret_cast<const uint16_t*>(data);
		return {
			glm::unpackHalf1x16(values[0]),
			glm::unpackHalf1x16(values[1]),
			glm::unpackHalf1x16(values[2]),
			glm::unpackHalf1x16(values[3])
		};
	}
	case VkFormat::VK_FORMAT_R8G8B8A8_UNORM:
	case VkFormat::VK_FORMAT_R8G8B8A8_SRGB:
		return glm::vec4{ data[0], data[1], data[2], data[3] } / 255.0f;
	default:
		throw std::runtime_error{ "BlitPass does not support the requested image format" };
	}
}