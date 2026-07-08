#pragma once
#include "VulkanContext.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "Mesh.h"
#include "FPSCamera.h"
#include "FrameContext.h"
#include "GLBLoader.h"
#include "pass/OpaquePass.h"
#include "pass/AtmospherePass.h"
#include "pass/CompositePass.h"

#include <glm/glm.hpp>

#include <memory>
#include <filesystem>
#include <array>

class VulkanBuffer;
class ImGUI;
class Window;
class Scene
{
public:
	Scene(VulkanContext& ctx, const ImGUI& imgui, Window& window);

	virtual void Init();
	virtual void Clear();
	virtual void Update(double dt);
	virtual void Render(double dt);
protected:
	virtual void CreateBuffers();
	virtual void SetupDescriptorPool();
	virtual void SetupDescriptor();
	virtual void InitFrameContext();
	virtual void CreateSyncObjects();

	virtual void BuildCommandBuffer();
	virtual void SubmitCommandBuffer();
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

	// 불칸 리소스들
	VkDescriptorPool descPool = VK_NULL_HANDLE;

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
	std::unique_ptr<Mesh<GLBVertex>> testMesh;
};