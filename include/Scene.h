#pragma once

#include "VulkanContext.h"

#include <filesystem>
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

	VkCommandBuffer cmd = VK_NULL_HANDLE;
private:
	VkShaderModule vertShader;
	VkShaderModule fragShader;

	struct
	{
		VkSemaphore imageAvailable = VK_NULL_HANDLE;
		VkSemaphore renderFinished = VK_NULL_HANDLE;
		VkSemaphore presentComplete = VK_NULL_HANDLE;
	} semaphores;
	VkFence inFlightFence = VK_NULL_HANDLE;

	uint32_t imgIdx = 0;
};