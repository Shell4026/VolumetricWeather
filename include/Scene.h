#pragma once
#include "GLBLoader.h"

#include "render/VulkanContext.h"
#include "render/VulkanBuffer.h"
#include "render/VulkanImage.h"
#include "render/Mesh.h"
#include "render/FrameContext.h"
#include "render/BarrierBuilder.h"

#include <glm/glm.hpp>

#include <memory>
#include <filesystem>
#include <array>
#include <vector>
#include <initializer_list>
class VulkanBuffer;
class ImGUI;
class Window;
class Material;
class Camera;
class AScene
{
public:
	AScene(VulkanContext& ctx, const ImGUI& imgui, Window& window);
	virtual ~AScene();

	void Init();
	virtual void Clear();
	virtual void Update(double dt) {};
	virtual void Render(double dt);

	auto GetCamera() const -> Camera* { return camera.get(); }
	auto GetCameraDescriptorSetLayout() const -> VkDescriptorSetLayout { return cameraDescSetLayout; }
	auto GetDescriptorPool() const -> VkDescriptorPool { return descPool; }
protected:
	virtual auto CreateDescriptorPool() -> VkDescriptorPool;
	virtual auto CreateSceneCamera() -> std::unique_ptr<Camera>;
	virtual void PrepareResource();
	virtual void SetupPass() {}
	void InitFrameContext();
	void CreateSyncObjects();

	virtual auto GetActivePassList() -> std::vector<APass*>& = 0;
	virtual void BeginBuildCommandBuffer() {}
	virtual void BuildCommandBuffer();
	virtual void SubmitCommandBuffer();

	void UpdateCameraData();
public:
	VulkanContext& ctx;
	const ImGUI& imgui;
	Window& window;
private:
	std::array<FrameContext, VulkanContext::MAX_CONCURRENT_FRAMES> frames;
	// 동기화 객체
	VkSemaphore timelineSemaphore = VK_NULL_HANDLE;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	uint64_t nextSubmitValue = 1;
	uint32_t currentImgIdx = 0;
	uint32_t currentFrameIdx = 0;
	BarrierBuilder barrierBuilder;

	// 불칸 리소스
	VkDescriptorPool descPool = VK_NULL_HANDLE;

	// 카메라
	std::unique_ptr<Camera> camera;
	struct CameraUniform
	{
		alignas(16) glm::vec3 pos;
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 proj;
	} cameraUniformData;
	std::unique_ptr<VulkanBuffer> cameraUniformBuffers;
	VkDescriptorSetLayout cameraDescSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet cameraDescSet{ VK_NULL_HANDLE };
};