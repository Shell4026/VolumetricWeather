#pragma once
#include "BarrierInfo.h"

#include <initializer_list>
#include <map>
class APass;
class VulkanImage;
class BarrierBuilder
{
public:
	auto BuildBarrier(const std::vector<APass*>& passes) -> std::vector<std::vector<BarrierInfo>>;
	auto BuildBarrier(std::initializer_list<const APass*> passes) -> std::vector<std::vector<BarrierInfo>>;
	void SetImageUsage(VkImage image, VkImageAspectFlags aspect, VkImageLayout layout, VkPipelineStageFlags stage, VkAccessFlags access);
	void InvalidateImage(VkImage image);
private:
	std::map<VkImage, std::vector<const ImageUsage*>> imageUsages;
	std::map<VkImage, ImageUsage> lastImageUsages;
};