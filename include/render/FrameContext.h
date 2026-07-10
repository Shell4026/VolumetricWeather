#pragma once
#include "vulkan/vulkan.h"
struct FrameContext
{
	VkDescriptorSetLayout cameraSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet cameraSet = VK_NULL_HANDLE;
	VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
	uint32_t imgIdx = 0;
	uint64_t submittedValue = 0;
};