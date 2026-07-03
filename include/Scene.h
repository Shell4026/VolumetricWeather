#pragma once
#include "VulkanContext.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
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
	virtual auto BeginFrame() -> bool;
	virtual void SubmitCommandBuffer();
	virtual void PrepareUniformBuffer();
	virtual void SetupDescriptorPool();
	virtual void SetupDescriptor();
	virtual void PrepareResource();

	static auto LoadShader(VkDevice device, const std::filesystem::path& path) -> VkShaderModule;
private:
	void SetupAtmosphereDescriptor();
	void CreateAtmospherePipeline();
	void BuildComputeCommandBuffer();
public:
	VulkanContext& ctx;
	const ImGUI& imgui;
protected:
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	VkCommandBuffer cmd = VK_NULL_HANDLE;
private:
	// 동기화 객체
	VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	VkFence inFlightFence = VK_NULL_HANDLE;

	// 불칸 리소스들
	VkShaderModule vertShader = VK_NULL_HANDLE;
	VkShaderModule fragShader = VK_NULL_HANDLE;

	VkDescriptorPool descPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout descSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet descSets{ VK_NULL_HANDLE };
	std::unique_ptr<VulkanBuffer> cameraUniformBuffers;
	std::unique_ptr<VulkanBuffer> uniformBuffers;
	struct Atomosphere
	{
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout descSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet descSets{ VK_NULL_HANDLE };

		std::unique_ptr<VulkanImage> storageImg;
		VkSampler storageImgSampler = VK_NULL_HANDLE;
		VkDescriptorImageInfo storageImgDescInfo;

		VkShaderModule computeShader = VK_NULL_HANDLE;

		//std::array<VkSemaphore>
		void Clear(const VulkanContext& ctx);
	} atomosphere;
	struct Compute
	{
		VkCommandBuffer cmd{ VK_NULL_HANDLE };
	} compute;
	
	uint32_t currentImgIdx = 0;

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