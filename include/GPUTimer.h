#pragma once
#include "VulkanContext.h"
class GPUTimer
{
public:
	GPUTimer();
	~GPUTimer();

	void Create(const VulkanContext& ctx);
	void Clear();

	void Begin(VkCommandBuffer cmd);
	void End(VkCommandBuffer cmd);

	auto GetElapsedMs() const -> double;
private:
	const VulkanContext* ctx = nullptr;

	float timestampPeriod = 1.f;

	VkQueryPool queryPool = VK_NULL_HANDLE;
};