#pragma once
#include "VulkanContext.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "Mesh.h"
#include "FPSCamera.h"
#include "FrameContext.h"
#include "GLBLoader.h"
#include "BarrierBuilder.h"
#include "pass/OpaquePass.h"
#include "pass/AtmospherePass.h"
#include "pass/CompositePass.h"

#include <glm/glm.hpp>

#include <memory>
#include <filesystem>
#include <array>
#include <vector>

class VulkanBuffer;
class ImGUI;
class Window;
class Material;
class Scene
{
public:
	Scene(VulkanContext& ctx, const ImGUI& imgui, Window& window);
	~Scene();

	void Init();
	virtual void Clear();
	virtual void Update(double dt);
	virtual void Render(double dt);
protected:
	virtual void SetupDescriptorPool();
	virtual void SetupPass();
	virtual void PrepareResource();
	virtual void SetupDescriptor();
	virtual void InitFrameContext();
	virtual void CreateSyncObjects();

	virtual void BuildCommandBuffer();
	virtual void SubmitCommandBuffer();
private:
	void CreateDrawables();
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

	// 불칸 리소스들
	VkDescriptorPool descPool = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;

	std::unique_ptr<OpaquePass> opaquePass;
	std::unique_ptr<AtmospherePass> atmospherePass;
	std::unique_ptr<CompositePass> compositePass;

	// 카메라
	FPSCamera camera;
	struct CameraUniform
	{
		alignas(16) glm::vec3 pos;
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 proj;
	} cameraUniformData;
	std::unique_ptr<VulkanBuffer> cameraUniformBuffers;
	VkDescriptorSetLayout cameraDescSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet cameraDescSet{ VK_NULL_HANDLE };

	//
	GLBLoader::Model mountainModel;
	
	struct MountainMaterialData
	{
		glm::vec4 color{ 1.f, 1.f, 1.f, 1.f };
	} mountainMaterialData;
	std::unique_ptr<Material> mountainMaterial;
	std::vector<Drawable> drawables;
};