#pragma once
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include "vulkan/vulkan.h"
struct ImageUsage
{
	VkImage image = VK_NULL_HANDLE;
	VkImageAspectFlags aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
	VkImageLayout layout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
	VkPipelineStageFlags stage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkAccessFlags access = VkAccessFlagBits::VK_ACCESS_NONE;
};

struct BarrierInfo
{
	VkImage image = VK_NULL_HANDLE;
	VkImageAspectFlags aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageLayout srcLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout dstLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
	VkPipelineStageFlags srcStage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkPipelineStageFlags dstStage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkAccessFlags srcAccess = VkAccessFlagBits::VK_ACCESS_NONE;
	VkAccessFlags dstAccess = VkAccessFlagBits::VK_ACCESS_NONE;
};