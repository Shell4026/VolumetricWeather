#pragma once

#include "vulkan/vulkan.h"
struct ImageUsage
{
	VkImage image = VK_NULL_HANDLE;
	VkImageAspectFlagBits aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
	VkImageLayout layout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
	VkPipelineStageFlagBits stage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkAccessFlagBits access = VkAccessFlagBits::VK_ACCESS_NONE;
};

struct BarrierInfo
{
	VkImage image = VK_NULL_HANDLE;
	VkImageAspectFlags aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageLayout srcLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout dstLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
	VkPipelineStageFlagBits srcStage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkPipelineStageFlagBits dstStage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkAccessFlagBits srcAccess = VkAccessFlagBits::VK_ACCESS_NONE;
	VkAccessFlagBits dstAccess = VkAccessFlagBits::VK_ACCESS_NONE;
};