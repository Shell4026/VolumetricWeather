#pragma once
#include "VulkanContext.h"
#include "VulkanBuffer.h"
#include "Mesh.h"
#include "Camera.h"

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
	virtual void PrepareResource();

	static auto LoadShader(VkDevice device, const std::filesystem::path& path) -> VkShaderModule;
public:
	VulkanContext& ctx;
	const ImGUI& imgui;
protected:
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	std::array<VkCommandBuffer, VulkanContext::MAX_CONCURRENT_FRAMES> cmd{ VK_NULL_HANDLE };
private:
	// 동기화 객체
	struct
	{
		VkSemaphore imageAvailable = VK_NULL_HANDLE;
	} semaphores[VulkanContext::MAX_CONCURRENT_FRAMES];
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::array<VkFence, VulkanContext::MAX_CONCURRENT_FRAMES> inFlightFence{ VK_NULL_HANDLE };

	// 불칸 리소스들
	VkShaderModule vertShader = VK_NULL_HANDLE;
	VkShaderModule fragShader = VK_NULL_HANDLE;

	VkDescriptorPool descPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout descSetLayout = VK_NULL_HANDLE;
	std::array<VkDescriptorSet, VulkanContext::MAX_CONCURRENT_FRAMES> descSets{ VK_NULL_HANDLE };
	std::array<std::unique_ptr<VulkanBuffer>, VulkanContext::MAX_CONCURRENT_FRAMES> cameraUniformBuffers;
	std::array<std::unique_ptr<VulkanBuffer>, VulkanContext::MAX_CONCURRENT_FRAMES> uniformBuffers;
	struct Compute
	{
		struct Image
		{
			VkSampler sampler = VK_NULL_HANDLE;
			VkImage img = VK_NULL_HANDLE;
			VkImageView view = VK_NULL_HANDLE;
			VkDeviceMemory mem = VK_NULL_HANDLE;
			VkDescriptorImageInfo descInfo;
			VkImageLayout layout = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL;
		} storageImg;
		
		VkPipeline pipeline = VK_NULL_HANDLE;
		std::array<VkCommandBuffer, VulkanContext::MAX_CONCURRENT_FRAMES> cmd{ VK_NULL_HANDLE };
	} compute;
	

	uint32_t currentImgIdx = 0;
	uint32_t currentFrame = 0;

	// 일반 리소스
	Camera camera;

	struct Vertex
	{
		glm::vec3 v;
	};
	Mesh<Vertex> plane;

	struct alignas(16) CameraUniform
	{
		glm::mat4 view;
		glm::mat4 proj;
	} cameraUniformData;
	struct alignas(16) Uniform
	{
		glm::vec4 color;
	} uniformData;

	glm::mat4 modelMatrix{ 1.f };
};