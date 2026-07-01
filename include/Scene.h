#pragma once

#include "VulkanContext.h"

#include <filesystem>
#include <array>
class Scene
{
public:
	Scene(VulkanContext& ctx);

	virtual void Init();
	virtual void Clear();
	virtual void Render();
protected:
	virtual void CreateSyncObjects();
	virtual void CreatePipeline();
	virtual void PrepareCommandBuffer();
	virtual void BuildCommandBuffer();
	virtual void SubmitCommandBuffer();
	virtual auto PrepareFrame() -> bool;

	static auto LoadShader(VkDevice device, const std::filesystem::path& path) -> VkShaderModule;
public:
	VulkanContext& ctx;
protected:
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	std::array<VkCommandBuffer, VulkanContext::MAX_CONCURRENT_FRAMES> cmd{ VK_NULL_HANDLE };
private:
	VkShaderModule vertShader = VK_NULL_HANDLE;
	VkShaderModule fragShader = VK_NULL_HANDLE;

	struct
	{
		VkSemaphore imageAvailable = VK_NULL_HANDLE;
	} semaphores[VulkanContext::MAX_CONCURRENT_FRAMES];
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::array<VkFence, VulkanContext::MAX_CONCURRENT_FRAMES> inFlightFence{ VK_NULL_HANDLE };

	uint32_t currentImgIdx = 0;
	uint32_t currentFrame = 0;
};