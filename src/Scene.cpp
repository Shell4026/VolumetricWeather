#include "Scene.h"
#include "Camera.h"

#include "core/Logger.h"
#include "core/Window.h"

#include "render/VulkanBuffer.h"
#include "render/Material.h"

#include "pass/APass.h"

#include "imgui/backends/imgui_impl_vulkan.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

#include <fstream>
#include <vector>
#include <cstdint>
#include <array>
#include <queue>

#ifdef max
#undef max
#endif
AScene::AScene(VulkanContext& ctx, const ImGUI& imgui, Window& window) :
	ctx(ctx), imgui(imgui), window(window)
{
}
AScene::~AScene()
{
	Clear();
}

void AScene::Init()
{
	descPool = CreateDescriptorPool();
	camera = std::move(CreateSceneCamera());
	PrepareResource();
	SetupPass();
	InitFrameContext();
	CreateSyncObjects();
}

void AScene::Clear()
{
	const VkDevice device = ctx.GetDevice();
	vkDeviceWaitIdle(device);

	vkDestroyDescriptorSetLayout(device, cameraDescSetLayout, nullptr);
	cameraDescSetLayout = VK_NULL_HANDLE;
	vkDestroyDescriptorPool(device, descPool, nullptr);
	descPool = VK_NULL_HANDLE;
	cameraDescSet = VK_NULL_HANDLE;

	for (FrameContext& frame : frames)
	{
		vkDestroySemaphore(device, frame.imageAvailableSemaphore, nullptr);
		frame.imageAvailableSemaphore = VK_NULL_HANDLE;
		frame.cameraSetLayout = VK_NULL_HANDLE;
		frame.cameraSet = VK_NULL_HANDLE;
	}
	for (VkSemaphore semaphore : renderFinishedSemaphores)
		vkDestroySemaphore(device, semaphore, nullptr);
	renderFinishedSemaphores.clear();
	vkDestroySemaphore(device, timelineSemaphore, nullptr);
	timelineSemaphore = VK_NULL_HANDLE;
}

void AScene::BeginRender(double dt)
{
	if (!ctx.AcquireNextImage(frames[currentFrameIdx].imageAvailableSemaphore, nullptr, &currentImgIdx))
		return;
	frames[currentFrameIdx].imgIdx = currentImgIdx;
	uint64_t recentlyValue = 0;
	for (FrameContext& frame : frames)
		recentlyValue = std::max(recentlyValue, frame.submittedValue);

	VkSemaphoreWaitInfo waitInfo{};
	waitInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	waitInfo.semaphoreCount = 1;
	waitInfo.pSemaphores = &timelineSemaphore;
	waitInfo.pValues = &recentlyValue;
	vkWaitSemaphores(ctx.GetDevice(), &waitInfo, UINT64_MAX);
}

void AScene::Render(double dt)
{
	cameraUniformBuffers->SetData(&cameraUniformData, sizeof(cameraUniformData));

	BeginBuildCommandBuffer();

	for (APass* pass : GetActivePassList())
		pass->SetUsages(ctx, frames[currentFrameIdx]);

	BuildCommandBuffer();
	SubmitCommandBuffer();
}

auto AScene::CreateDescriptorPool()->VkDescriptorPool
{
	VkDescriptorPool descPool = VK_NULL_HANDLE;

	std::vector<VkDescriptorPoolSize> poolSizes;
	VkDescriptorPoolSize& uniformBufferPoolSize = poolSizes.emplace_back();
	uniformBufferPoolSize.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniformBufferPoolSize.descriptorCount = 10;
	VkDescriptorPoolSize& storageImagePoolSize = poolSizes.emplace_back();
	storageImagePoolSize.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	storageImagePoolSize.descriptorCount = 10;
	VkDescriptorPoolSize& samplerPoolSize = poolSizes.emplace_back();
	samplerPoolSize.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerPoolSize.descriptorCount = 10;

	VkDescriptorPoolCreateInfo poolCi{};
	poolCi.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCi.maxSets = 10;
	poolCi.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolCi.pPoolSizes = poolSizes.data();
	poolCi.flags = VkDescriptorPoolCreateFlagBits::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	VK_RESULT_CHECK(vkCreateDescriptorPool(ctx.GetDevice(), &poolCi, nullptr, &descPool));
	return descPool;
}

auto AScene::CreateSceneCamera() -> std::unique_ptr<Camera>
{
	std::unique_ptr<Camera> cameraPtr = std::make_unique<Camera>();
	cameraPtr->SetPos(glm::vec3{ 0.f, 100.f, 0.f });
	cameraPtr->SetTo(glm::vec3{ 0.f, 100.f, -1.f });
	cameraPtr->UpdateMatrix();
	cameraUniformData.pos = cameraPtr->GetPos();
	cameraUniformData.view = cameraPtr->GetMatrixView();
	cameraUniformData.proj = cameraPtr->GetMatrixProj();

	return cameraPtr;
}

void AScene::PrepareResource()
{
	const VkMemoryPropertyFlags memProp = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	cameraUniformBuffers =
		std::make_unique<VulkanBuffer>(VulkanBuffer::Create(ctx, VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, memProp, sizeof(cameraUniformData), &cameraUniformData));

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
	VkDescriptorSetLayoutBinding& binding0 = setLayoutBindings.emplace_back();
	binding0.binding = 0;
	binding0.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT | VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT | VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	binding0.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding0.descriptorCount = 1;

	VkDescriptorSetLayoutCreateInfo layoutCi{};
	layoutCi.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCi.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
	layoutCi.pBindings = setLayoutBindings.data();
	VK_RESULT_CHECK(vkCreateDescriptorSetLayout(ctx.GetDevice(), &layoutCi, nullptr, &cameraDescSetLayout));

	VkDescriptorSetAllocateInfo descSetAllocInfo{};
	descSetAllocInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descSetAllocInfo.descriptorPool = descPool;
	descSetAllocInfo.pSetLayouts = &cameraDescSetLayout;
	descSetAllocInfo.descriptorSetCount = 1;
	VK_RESULT_CHECK(vkAllocateDescriptorSets(ctx.GetDevice(), &descSetAllocInfo, &cameraDescSet));

	VkWriteDescriptorSet writeSet{};
	writeSet.sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.dstSet = cameraDescSet;
	writeSet.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeSet.descriptorCount = 1;
	writeSet.dstBinding = 0;
	writeSet.pBufferInfo = &cameraUniformBuffers->GetDescriptorBufferInfo();
	vkUpdateDescriptorSets(ctx.GetDevice(), 1, &writeSet, 0, nullptr);
}

void AScene::InitFrameContext()
{
	VkSemaphoreCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	for (FrameContext& frame : frames)
	{
		frame.cameraSetLayout = cameraDescSetLayout;
		frame.cameraSet = cameraDescSet;
		VK_RESULT_CHECK(vkCreateSemaphore(ctx.GetDevice(), &ci, nullptr, &frame.imageAvailableSemaphore));
	}
}

void AScene::CreateSyncObjects()
{
	VkSemaphoreCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	renderFinishedSemaphores.resize(ctx.GetSwapChainImages().size());
	for (VkSemaphore& semaphore : renderFinishedSemaphores)
		VK_RESULT_CHECK(vkCreateSemaphore(ctx.GetDevice(), &ci, nullptr, &semaphore));

	// 타임라인 세마포어
	VkSemaphoreTypeCreateInfo typeCI{};
	typeCI.sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	typeCI.initialValue = 0;
	typeCI.semaphoreType = VkSemaphoreType::VK_SEMAPHORE_TYPE_TIMELINE;
	ci.pNext = &typeCI;
	VK_RESULT_CHECK(vkCreateSemaphore(ctx.GetDevice(), &ci, nullptr, &timelineSemaphore));
	ci.pNext = nullptr;
}

void AScene::BuildCommandBuffer()
{
	std::vector<APass*>& activePassList = GetActivePassList();
	const std::vector<std::vector<BarrierInfo>> barriers = barrierBuilder.BuildBarrier(activePassList);

	FrameContext& frame = frames[currentFrameIdx];

	std::vector<BarrierInfo> endBarrier;
	BarrierInfo& barrier = endBarrier.emplace_back();
	barrier.image = ctx.GetSwapChainImages()[frame.imgIdx];
	barrier.aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.srcLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	barrier.dstLayout = VkImageLayout::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.srcAccess = VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrier.dstAccess = VkAccessFlagBits::VK_ACCESS_NONE;
	barrier.srcStage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	barrier.dstStage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

	int idx = 0;
	for (APass* pass : activePassList)
	{
		pass->BeginRecord(&barriers[idx]);
		pass->Record(ctx, frame);
		if (idx == activePassList.size() - 1)
			pass->EndRecord(&endBarrier);
		else
			pass->EndRecord();
		++idx;
	}
}

void AScene::SubmitCommandBuffer()
{
	FrameContext& frame = frames[currentFrameIdx];
	frame.submittedValue = nextSubmitValue++;

	std::vector<APass*>& activePassList = GetActivePassList();
	const APass* firstSwapchainPass = nullptr;
	int idx = 0;
	for (APass* pass : activePassList)
	{
		if (pass == nullptr)
			continue;
		VkSubmitInfo info = pass->GetSubmitInfo();
		std::array<VkSemaphore, 10> waits{ VK_NULL_HANDLE };
		std::array<VkSemaphore, 10> signals{ VK_NULL_HANDLE };
		std::array<VkPipelineStageFlags, 10> waitStages{ 0 };
		std::array<uint64_t, 10> waitTimelineValues{ 0 };
		std::array<uint64_t, 10> signalTimelineValues{ 0 };
		if (info.waitSemaphoreCount > 0)
		{
			for (std::size_t i = 0; i < info.waitSemaphoreCount; ++i)
			{
				waits[i] = *(info.pWaitSemaphores + i);
				waitStages[i] = *(info.pWaitDstStageMask + i);
			}
			if (info.pNext != nullptr)
			{
				const VkTimelineSemaphoreSubmitInfo* tsInfo = reinterpret_cast<const VkTimelineSemaphoreSubmitInfo*>(info.pNext);
				if (tsInfo->sType == VkStructureType::VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO)
				{
					for (std::size_t i = 0; i < info.waitSemaphoreCount; ++i)
						waitTimelineValues[i] = *(tsInfo->pWaitSemaphoreValues + i);
				}
			}
		}
		if (info.signalSemaphoreCount > 0)
		{
			for (std::size_t i = 0; i < info.signalSemaphoreCount; ++i)
				signals[i] = *(info.pSignalSemaphores + i);
			if (info.pNext != nullptr)
			{
				const VkTimelineSemaphoreSubmitInfo* tsInfo = reinterpret_cast<const VkTimelineSemaphoreSubmitInfo*>(info.pNext);
				if (tsInfo->sType == VkStructureType::VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO)
				{
					for (std::size_t i = 0; i < info.signalSemaphoreCount; ++i)
						signalTimelineValues[i] = *(tsInfo->pSignalSemaphoreValues + i);
				}
			}
		}

		// 처음 스왑체인에 쓰는 패스
		if (firstSwapchainPass == nullptr && pass->IsUsingSwapchainImage())
		{
			waits[info.waitSemaphoreCount] = frame.imageAvailableSemaphore;
			waitStages[info.waitSemaphoreCount] = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

			VkTimelineSemaphoreSubmitInfo tsInfo{};
			tsInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
			tsInfo.waitSemaphoreValueCount = waits.size();
			tsInfo.pWaitSemaphoreValues = waitTimelineValues.data();

			info.waitSemaphoreCount += 1;
			info.pWaitSemaphores = waits.data();
			info.pWaitDstStageMask = waitStages.data();
			info.pNext = &tsInfo;
			firstSwapchainPass = pass;
			VK_RESULT_CHECK(vkQueueSubmit(ctx.GetGraphicsQueue(), 1, &info, pass->GetFence()));
			++idx;
			continue;
		}
		// 마지막 패스
		if (idx == activePassList.size() - 1)
		{
			signals[info.signalSemaphoreCount] = timelineSemaphore;
			signals[info.signalSemaphoreCount + 1] = renderFinishedSemaphores[currentImgIdx];
			signalTimelineValues[info.signalSemaphoreCount] = frame.submittedValue;
			info.signalSemaphoreCount += 2;

			VkTimelineSemaphoreSubmitInfo tsInfo{};
			tsInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
			tsInfo.signalSemaphoreValueCount = info.signalSemaphoreCount;
			tsInfo.pSignalSemaphoreValues = signalTimelineValues.data();

			info.pSignalSemaphores = signals.data();
			info.pNext = &tsInfo;
			VK_RESULT_CHECK(vkQueueSubmit(ctx.GetGraphicsQueue(), 1, &info, pass->GetFence()));
			break;
		}
		VK_RESULT_CHECK(vkQueueSubmit(ctx.GetGraphicsQueue(), 1, &info, pass->GetFence()));
		++idx;
	}

	VkSwapchainKHR swapChains[] = { ctx.GetSwapChain() };
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentImgIdx];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &currentImgIdx;
	vkQueuePresentKHR(ctx.GetPresentQueue(), &presentInfo);

	currentFrameIdx = (currentFrameIdx + 1) % VulkanContext::MAX_CONCURRENT_FRAMES;
}

void AScene::UpdateCameraData()
{
	if (camera == nullptr)
		return;
	cameraUniformData.pos = camera->GetPos();
	cameraUniformData.view = camera->GetMatrixView();
	cameraUniformData.proj = camera->GetMatrixProj();
	cameraUniformData.invView = camera->GetMatrixInverseView();
	cameraUniformData.invProj = camera->GetMatrixInverseProj();
}