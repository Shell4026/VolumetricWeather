#include "VulkanImage.h"
#include "Logger.h"

auto VulkanSampler::Create(const VulkanContext& ctx, const VkSamplerCreateInfo& ci) -> VulkanSampler
{
	VulkanSampler sampler{};
	sampler.device = ctx.GetDevice();
	sampler.info = ci;
	VK_RESULT_CHECK(vkCreateSampler(sampler.device, &ci, nullptr, &sampler.sampler));
	return sampler;
}

VulkanSampler::~VulkanSampler()
{
	if (sampler == VK_NULL_HANDLE)
		return;
	vkDestroySampler(device, sampler, nullptr);
}

auto VulkanImage::Create(const VulkanContext& ctx, const VkImageCreateInfo& ci, VkImageAspectFlags aspect, VkMemoryPropertyFlags memProp) -> VulkanImage
{
	VulkanImage img{};
	img.device = ctx.GetDevice();
	img.info = ci;
	VK_RESULT_CHECK(vkCreateImage(img.device, &ci, nullptr, &img.img));

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(img.device, img.img, &memReqs);
	VkMemoryAllocateInfo memAllocInfo{};
	memAllocInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = ctx.FindMemoryType(memReqs.memoryTypeBits, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_RESULT_CHECK(vkAllocateMemory(img.device, &memAllocInfo, nullptr, &img.mem));
	VK_RESULT_CHECK(vkBindImageMemory(img.device, img.img, img.mem, 0));

	VkImageViewCreateInfo viewCi{};
	viewCi.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCi.viewType = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
	viewCi.format = img.info.format;
	viewCi.subresourceRange = { aspect, 0, 1, 0, 1 };
	viewCi.image = img.img;
	VK_RESULT_CHECK(vkCreateImageView(img.device, &viewCi, nullptr, &img.view));

	return img;
}

VulkanImage::~VulkanImage()
{
	if (img == VK_NULL_HANDLE)
		return;
	vkDestroyImageView(device, view, nullptr);
	vkFreeMemory(device, mem, nullptr);
	vkDestroyImage(device, img, nullptr);
}
