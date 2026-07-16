#pragma once
#include "pass/APass.h"

#include "glm/vec4.hpp"

#include <vector>
#include <cstdint>
#include <thread>
#include <atomic>
#include <future>
#include <mutex>
#include <memory>
class VulkanImage;
class VulkanBuffer;
class BlitPass : public APass
{
public:
	BlitPass();
	~BlitPass();

	void Clear() override;

	void Record(const VulkanContext& ctx, const FrameContext& frame) override;

	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;

	auto RequestBlit(const VulkanImage& img, int32_t x, int32_t y) -> std::future<glm::vec4>;
protected:
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;
	void BuildPipeline(const VulkanContext& ctx) override {}
private:
	void CreateBuffer();
public:
	inline static constexpr std::size_t CAPACITY = 10;
private:
	std::thread thr;
	std::mutex mu;
	VkSemaphore timeline = VK_NULL_HANDLE;

	struct BlitRequest
	{
		const VulkanImage* img = nullptr;
		VkFence fence = VK_NULL_HANDLE;
		std::promise<glm::vec4> promise;
		int32_t x = 0;
		int32_t y = 0;
		uint64_t requestTimelineValue = 0;
	};
	std::vector<BlitRequest> blitQueue;
	std::atomic_bool bHasRequest{ false };

	std::unique_ptr<VulkanBuffer> buffer;

	VkTimelineSemaphoreSubmitInfo ts{};
	uint64_t timelineValue = 0;

	bool bStopThread = false;
};