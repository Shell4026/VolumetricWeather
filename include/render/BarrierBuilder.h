#pragma once
#include "BarrierInfo.h"

#include <initializer_list>
#include <map>
class APass;
class VulkanImage;
class BarrierBuilder
{
public:
	auto BuildBarrier(std::initializer_list<const APass*> passes) -> std::vector<std::vector<BarrierInfo>>;
private:
	std::map<VkImage, std::vector<const ImageUsage*>> imageUsages;
};