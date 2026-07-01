#pragma once

#include "VulkanContext.h"
#include "VulkanBuffer.h"

#include <glm/glm.hpp>

#include <memory>
#include <filesystem>
#include <array>

class VulkanBuffer;
class ImGUI;
class Scene
{
public:
	Scene(VulkanContext& ctx, const ImGUI& imgui);

	virtual void Init();
	virtual void Clear();
	virtual void Render(double dt);
protected:
	virtual void CreateSyncObjects();
	virtual void CreatePipeline();
	virtual void PrepareCommandBuffer();
	virtual void BuildCommandBuffer();
	virtual void SubmitCommandBuffer();
	virtual auto PrepareFrame() -> bool;
	virtual void PrepareUniformBuffer();
	virtual void SetupDescriptor();

	static auto LoadShader(VkDevice device, const std::filesystem::path& path) -> VkShaderModule;
public:
	VulkanContext& ctx;
	const ImGUI& imgui;
protected:
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	std::array<VkCommandBuffer, VulkanContext::MAX_CONCURRENT_FRAMES> cmd{ VK_NULL_HANDLE };
private:
	VkShaderModule vertShader = VK_NULL_HANDLE;
	VkShaderModule fragShader = VK_NULL_HANDLE;

	VkDescriptorPool descPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout descSetLayout = VK_NULL_HANDLE;
	std::array<VkDescriptorSet, VulkanContext::MAX_CONCURRENT_FRAMES> descSets{ VK_NULL_HANDLE };
	struct UniformData
	{
		glm::vec4 color;
	} uniformData;
	std::array<std::unique_ptr<VulkanBuffer>, VulkanContext::MAX_CONCURRENT_FRAMES> uniformBuffers;

	struct
	{
		VkSemaphore imageAvailable = VK_NULL_HANDLE;
	} semaphores[VulkanContext::MAX_CONCURRENT_FRAMES];
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::array<VkFence, VulkanContext::MAX_CONCURRENT_FRAMES> inFlightFence{ VK_NULL_HANDLE };

	uint32_t currentImgIdx = 0;
	uint32_t currentFrame = 0;
};