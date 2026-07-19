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

	auto RequestBlit(const VulkanImage& img, int32_t x, int32_t y, uint32_t width = 1, uint32_t height = 1) -> std::future<std::vector<glm::vec4>>;
protected:
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;
	void BuildPipeline(const VulkanContext& ctx) override {}
private:
	auto CreateBuffer(std::size_t size) const -> std::unique_ptr<VulkanBuffer>;
	static auto DecodeTexel(const uint8_t* data, VkFormat format) -> glm::vec4;
public:
	inline static constexpr std::size_t CAPACITY = 10;
private:
	std::thread thr;
	std::mutex mu;
	VkSemaphore timeline = VK_NULL_HANDLE;

	struct BlitRequest
	{
		VkImage image = VK_NULL_HANDLE;
		VkFormat format = VkFormat::VK_FORMAT_UNDEFINED;
		std::unique_ptr<VulkanBuffer> buffer;
		std::promise<std::vector<glm::vec4>> promise;
		int32_t x = 0;
		int32_t y = 0;
		uint32_t width = 0;
		uint32_t height = 0;
		uint64_t completionValue = 0;
	};
	std::vector<BlitRequest> pendingRequests;
	std::vector<BlitRequest> submittedRequests;
	std::atomic_bool bHasRequest{ false };

	VkTimelineSemaphoreSubmitInfo ts{};
	uint64_t timelineValue = 0;

	std::atomic_bool bStopThread{ false };
};
