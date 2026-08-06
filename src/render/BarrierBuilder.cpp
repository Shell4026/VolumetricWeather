#include "render/BarrierBuilder.h"
#include "pass/APass.h"

auto BarrierBuilder::BuildBarrier(const std::vector<APass*>& passes) -> std::vector<std::vector<BarrierInfo>>
{
	imageUsages.clear();

	int passIdx = 0;
	for (const APass* pass : passes)
	{
		if (pass == nullptr)
		{
			++passIdx;
			continue;
		}

		for (const ImageUsage& usage : pass->GetUsages())
		{
			auto it = imageUsages.find(usage.image);
			if (it == imageUsages.end())
			{
				imageUsages[usage.image].resize(passes.size());
				it = imageUsages.find(usage.image);
			}
			it->second[passIdx] = &usage;
		}
		++passIdx;
	}

	std::vector<std::vector<BarrierInfo>> barriers(passes.size() + 1);
	for (auto& [img, usages] : imageUsages)
	{
		const ImageUsage* lastUsagePtr = nullptr;
		if (auto it = lastImageUsages.find(img); it != lastImageUsages.end())
			lastUsagePtr = &it->second;
		for (int passIdx = 0; passIdx < usages.size(); ++passIdx)
		{
			const ImageUsage* usagePtr = usages[passIdx];
			if (usagePtr == nullptr)
				continue;

			BarrierInfo barrier{};
			barrier.aspect = usagePtr->aspect;
			barrier.image = img;
			barrier.dstStage = usagePtr->stage;
			barrier.dstAccess = usagePtr->access;
			barrier.dstLayout = usagePtr->layout;
			if (lastUsagePtr == nullptr)
			{
				barrier.srcStage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;;
				barrier.srcAccess = 0;
				barrier.srcLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
			}
			else
			{
				barrier.srcStage = lastUsagePtr->stage;
				barrier.srcAccess = lastUsagePtr->access;
				barrier.srcLayout = lastUsagePtr->layout;
			}
			barriers[passIdx].push_back(barrier);
			lastUsagePtr = usagePtr;
		}
		if (lastUsagePtr != nullptr)
			lastImageUsages.insert_or_assign(img, *lastUsagePtr);
	}
	return barriers;
}


auto BarrierBuilder::BuildBarrier(std::initializer_list<const APass*> passes) -> std::vector<std::vector<BarrierInfo>>
{
	imageUsages.clear();

	int passIdx = 0;
	for (const APass* pass : passes)
	{
		if (pass == nullptr)
		{
			++passIdx;
			continue;
		}

		for (const ImageUsage& usage : pass->GetUsages())
		{
			auto it = imageUsages.find(usage.image);
			if (it == imageUsages.end())
			{
				imageUsages[usage.image].resize(passes.size());
				it = imageUsages.find(usage.image);
			}
			it->second[passIdx] = &usage;
		}
		++passIdx;
	}

	std::vector<std::vector<BarrierInfo>> barriers(passes.size() + 1);
	for (auto& [img, usages] : imageUsages)
	{
		const ImageUsage* lastUsagePtr = nullptr;
		if (auto it = lastImageUsages.find(img); it != lastImageUsages.end())
			lastUsagePtr = &it->second;
		for (int passIdx = 0; passIdx < usages.size(); ++passIdx)
		{
			const ImageUsage* usagePtr = usages[passIdx];
			if (usagePtr == nullptr)
				continue;
			
			BarrierInfo barrier{};
			barrier.aspect = usagePtr->aspect;
			barrier.image = img;
			barrier.dstStage = usagePtr->stage;
			barrier.dstAccess = usagePtr->access;
			barrier.dstLayout = usagePtr->layout;
			if (lastUsagePtr == nullptr)
			{
				barrier.srcStage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;;
				barrier.srcAccess = 0;
				barrier.srcLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
			}
			else
			{
				barrier.srcStage = lastUsagePtr->stage;
				barrier.srcAccess = lastUsagePtr->access;
				barrier.srcLayout = lastUsagePtr->layout;
			}
			barriers[passIdx].push_back(barrier);
			lastUsagePtr = usagePtr;
		}
		if (lastUsagePtr != nullptr)
			lastImageUsages.insert_or_assign(img, *lastUsagePtr);
	}
	return barriers;
}

void BarrierBuilder::SetImageUsage(VkImage image, VkImageAspectFlags aspect, VkImageLayout layout, VkPipelineStageFlags stage, VkAccessFlags access)
{
	ImageUsage usage{};
	usage.image = image;
	usage.aspect = aspect;
	usage.layout = layout;
	usage.stage = stage;
	usage.access = access;
	lastImageUsages.insert_or_assign(image, usage);
}

void BarrierBuilder::InvalidateImage(VkImage image)
{
	lastImageUsages.erase(image);
}