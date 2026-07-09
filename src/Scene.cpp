#include "Scene.h"
#include "core/Logger.h"
#include "core/Window.h"
#include "core/Input.h"
#include "VulkanBuffer.h"

#include "imgui/backends/imgui_impl_vulkan.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

#include <fstream>
#include <vector>
#include <cstdint>
#include <array>
#include <queue>
Scene::Scene(VulkanContext& ctx, const ImGUI& imgui, Window& window) :
	ctx(ctx), imgui(imgui), window(window)
{
}
void Scene::Init()
{
	camera.SetPos(glm::vec3{ 0.f, 100.f, 0.f });
	camera.SetYaw(-90.f);
	camera.SetPitch(0.f);
	camera.UpdateMatrix();
	cameraUniformData.pos = camera.GetPos();
	cameraUniformData.view = camera.GetMatrixView();
	cameraUniformData.proj = camera.GetMatrixProj();

	CreateDrawables();
	CreateBuffers();
	SetupDescriptorPool();
	SetupDescriptor();
	InitFrameContext();
	CreateSyncObjects();

	opaquePass = std::make_unique<OpaquePass>();
	opaquePass->Init(ctx, descPool, cameraDescSetLayout);

	atmospherePass = std::make_unique<AtmospherePass>();
	atmospherePass->SetOpaqueDepthTexture(*opaquePass->GetOutputImageDepth());
	atmospherePass->Init(ctx, descPool, cameraDescSetLayout);

	compositePass = std::make_unique<CompositePass>(*opaquePass->GetOutputImage(), *atmospherePass->GetOutputImage());
	compositePass->Init(ctx, descPool, cameraDescSetLayout);
}

void Scene::Clear()
{
	vkDeviceWaitIdle(ctx.GetDevice());

	compositePass->Clear(ctx, descPool);
	atmospherePass->Clear(ctx, descPool);
	opaquePass->Clear(ctx, descPool);

	vkDestroyDescriptorSetLayout(ctx.GetDevice(), cameraDescSetLayout, nullptr);
	cameraDescSetLayout = VK_NULL_HANDLE;
	vkDestroyDescriptorPool(ctx.GetDevice(), descPool, nullptr);
	descPool = VK_NULL_HANDLE;
	cameraDescSet = VK_NULL_HANDLE;

	for (FrameContext& frame : frames)
	{
		vkDestroySemaphore(ctx.GetDevice(), frame.imageAvailableSemaphore, nullptr);
		frame.imageAvailableSemaphore = VK_NULL_HANDLE;
		frame.cameraSetLayout = VK_NULL_HANDLE;
		frame.cameraSet = VK_NULL_HANDLE;
	}
	for (VkSemaphore semaphore : renderFinishedSemaphores)
		vkDestroySemaphore(ctx.GetDevice(), semaphore, nullptr);
	renderFinishedSemaphores.clear();
	vkDestroySemaphore(ctx.GetDevice(), timelineSemaphore, nullptr);
	timelineSemaphore = VK_NULL_HANDLE;
}

void Scene::Update(double dt)
{
	atmospherePass->Update(dt);
	compositePass->Update(dt);

	if (Input::IsKeyDown(Event::KeyType::W))
	{
		camera.AddPitch(30.0 * dt);
		camera.UpdateMatrix();
		cameraUniformData.pos = camera.GetPos();
		cameraUniformData.view = camera.GetMatrixView();
		cameraUniformData.proj = camera.GetMatrixProj();
	}
	if (Input::IsKeyDown(Event::KeyType::S))
	{
		camera.AddPitch(-30.0 * dt);
		camera.UpdateMatrix();
		cameraUniformData.pos = camera.GetPos();
		cameraUniformData.view = camera.GetMatrixView();
		cameraUniformData.proj = camera.GetMatrixProj();
	}
	if (Input::IsKeyDown(Event::KeyType::A))
	{
		camera.AddYaw(-30.0 * dt);
		camera.UpdateMatrix();
		cameraUniformData.pos = camera.GetPos();
		cameraUniformData.view = camera.GetMatrixView();
		cameraUniformData.proj = camera.GetMatrixProj();
	}
	if (Input::IsKeyDown(Event::KeyType::D))
	{
		camera.AddYaw(30.0 * dt);
		camera.UpdateMatrix();
		cameraUniformData.pos = camera.GetPos();
		cameraUniformData.view = camera.GetMatrixView();
		cameraUniformData.proj = camera.GetMatrixProj();
	}
	if (Input::IsKeyDown(Event::KeyType::Space))
	{
		glm::vec3 pos = camera.GetPos();
		pos.y += 1000.0 * dt;
		camera.SetPos(pos);
		camera.CalcTo();
		camera.UpdateMatrix();
		cameraUniformData.pos = camera.GetPos();
		cameraUniformData.view = camera.GetMatrixView();
		cameraUniformData.proj = camera.GetMatrixProj();
	}
	if (Input::IsKeyDown(Event::KeyType::LCtrl))
	{
		glm::vec3 pos = camera.GetPos();
		pos.y -= 1000.0 * dt;
		camera.SetPos(pos);
		camera.CalcTo();
		camera.UpdateMatrix();
		cameraUniformData.pos = camera.GetPos();
		cameraUniformData.view = camera.GetMatrixView();
		cameraUniformData.proj = camera.GetMatrixProj();
	}

	ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_::ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_::ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_::ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_::ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_::ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_::ImGuiWindowFlags_NoInputs;
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
	if (ImGui::Begin("Overlay", nullptr, windowFlags))
	{
		const glm::vec3& pos = camera.GetPos();
		const glm::vec3& to = camera.GetTo();
		ImGui::Text(std::format("pos: {:.2f}, {:.2f}, {:.2f}", pos.x, pos.y, pos.z).c_str());
		ImGui::Text(std::format("to: {:.2f}, {:.2f}, {:.2f}", to.x, to.y, to.z).c_str());
	}
	ImGui::End();

	ImGui::SetNextWindowSize(ImVec2{ 300.f, 300.f }, ImGuiCond_::ImGuiCond_Appearing);
	if (ImGui::Begin("Debug"))
	{
		AtmospherePass::Atmosphere atmosphere = atmospherePass->GetAtmosphere();
		ImGui::Text("Steps");
		if (ImGui::SliderInt("##steps", &atmosphere.steps, 1, 512))
			atmospherePass->SetAtmosphere(atmosphere);

		ImGui::Text("Sun illuminance");
		if (ImGui::SliderFloat("##SunIlluminance", &atmosphere.sun.w, 0.f, 1000.f))
			atmospherePass->SetAtmosphere(atmosphere);

		ImGui::Text("Sun direction");
		static float angle = 0.0f;
		if (ImGui::SliderFloat("##SunDirection", &angle, 0.f, 360.f))
		{
			glm::quat q = glm::quat{ glm::vec3(0.f, 0.f, glm::radians(angle)) };
			glm::vec3 sunDir = q * glm::normalize(glm::vec3{ -1.f, 0.f, -1.f });
			atmosphere.sun = glm::vec4(sunDir, atmosphere.sun.w);
			atmospherePass->SetAtmosphere(atmosphere);
		}

		ImGui::Text("Cam pos");
		float pos[] = { camera.GetPos().x, camera.GetPos().y, camera.GetPos().z };
		if (ImGui::InputFloat3("##camPos", pos, "%.3f", ImGuiInputTextFlags_::ImGuiInputTextFlags_EnterReturnsTrue))
		{
			camera.SetPos({ pos[0], pos[1], pos[2] });
			camera.CalcTo();
			camera.UpdateMatrix();
			cameraUniformData.pos = camera.GetPos();
			cameraUniformData.view = camera.GetMatrixView();
			cameraUniformData.proj = camera.GetMatrixProj();
		}

		ImGui::End();
	}
}

void Scene::Render(double dt)
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

	cameraUniformBuffers->SetData(&cameraUniformData);

	for (const Drawable& drawable : drawables)
		opaquePass->PushDrawable(drawable);

	opaquePass->SetUsages(ctx, frames[currentFrameIdx]);
	atmospherePass->SetUsages(ctx, frames[currentFrameIdx]);
	compositePass->SetUsages(ctx, frames[currentFrameIdx]);
	BuildCommandBuffer();
	SubmitCommandBuffer();
}

void Scene::CreateBuffers()
{
	const VkMemoryPropertyFlags memProp = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	cameraUniformBuffers = 
		std::make_unique<VulkanBuffer>(VulkanBuffer::Create(ctx, VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, memProp, sizeof(cameraUniformData), &cameraUniformData));
}

void Scene::SetupDescriptorPool()
{
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
	VK_RESULT_CHECK(vkCreateDescriptorPool(ctx.GetDevice(), &poolCi, nullptr, &descPool));
}

void Scene::SetupDescriptor()
{
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

void Scene::InitFrameContext()
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

void Scene::CreateSyncObjects()
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

void Scene::BuildCommandBuffer()
{
	std::vector<std::vector<BarrierInfo>> barriers = barrierBuilder.BuildBarrier({ opaquePass.get(), atmospherePass.get(), compositePass.get() });

	FrameContext& frame = frames[currentFrameIdx];
	{
		opaquePass->BeginRecord(&barriers[0]);
		opaquePass->Record(ctx, frame);
		opaquePass->EndRecord();
	}
	{
		atmospherePass->BeginRecord(&barriers[1]);
		atmospherePass->Record(ctx, frame);
		atmospherePass->EndRecord();
	}
	{
		std::vector<BarrierInfo> postBarriers;
		for (const ImageUsage& usage : compositePass->GetUsages())
		{
			BarrierInfo& barrier = postBarriers.emplace_back();
			barrier.image = usage.image;
			barrier.aspect = usage.aspect;
			barrier.srcLayout = usage.layout;
			barrier.dstLayout = VkImageLayout::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			barrier.srcAccess = usage.access;
			barrier.dstAccess = VkAccessFlagBits::VK_ACCESS_NONE;
			barrier.srcStage = usage.stage;
			barrier.dstStage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		}

		compositePass->BeginRecord(&barriers[2]);
		compositePass->Record(ctx, frame);
		compositePass->EndRecord(&postBarriers);
	}
}

void Scene::SubmitCommandBuffer()
{
	FrameContext& frame = frames[currentFrameIdx];
	frame.submittedValue = nextSubmitValue++;

	{
		const VkCommandBuffer cmd = opaquePass->GetCommandBuffer();
		VkSubmitInfo info{};
		info.sType = VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &cmd;
		VK_RESULT_CHECK(vkQueueSubmit(ctx.GetGraphicsQueue(), 1, &info, nullptr));
	}
	{
		const VkCommandBuffer cmd = atmospherePass->GetCommandBuffer();
		VkSubmitInfo info{};
		info.sType = VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &cmd;
		VK_RESULT_CHECK(vkQueueSubmit(ctx.GetGraphicsQueue(), 1, &info, nullptr));
	}
	{
		const VkCommandBuffer cmd = compositePass->GetCommandBuffer();

		VkPipelineStageFlags waitStages[] = { VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		std::array<VkSemaphore, 2> signals = { timelineSemaphore, renderFinishedSemaphores[currentImgIdx] };
		std::array<uint64_t, 2> values = { frame.submittedValue, 0 };

		VkTimelineSemaphoreSubmitInfo tsInfo{};
		tsInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
		tsInfo.signalSemaphoreValueCount = signals.size();
		tsInfo.pSignalSemaphoreValues = values.data();

		VkSubmitInfo info{};
		info.sType = VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &cmd;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &frame.imageAvailableSemaphore;
		info.signalSemaphoreCount = signals.size();
		info.pSignalSemaphores = signals.data();
		info.pWaitDstStageMask = waitStages;
		info.pNext = &tsInfo;
		VK_RESULT_CHECK(vkQueueSubmit(ctx.GetGraphicsQueue(), 1, &info, nullptr));
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

void Scene::CreateDrawables()
{
	mountainModel = GLBLoader::LoadGLB(ctx, "models/mountain_1.glb");
	//mountainNodes[0].modelMatrix = glm::translate(glm::mat4{ 1.f }, glm::vec3{ 0.f, 0.f, -5.f });
	struct BFSInfo
	{
		GLBLoader::Node* node;
		glm::mat4 parentModelMatrix;
	};
	std::queue<BFSInfo> bfs;
	bfs.push({ &mountainModel.nodes[0], glm::mat4{1.f} });
	while (!bfs.empty())
	{
		auto [nodePtr, parentModelMatrix] = bfs.front();
		bfs.pop();

		Drawable& drawable = drawables.emplace_back();
		drawable.modelMatrix = parentModelMatrix * nodePtr->modelMatrix;
		drawable.mesh = nodePtr->meshPtr.get();

		for (int idx : nodePtr->childrenIdxs)
		{
			GLBLoader::Node& child = mountainModel.nodes[idx];
			bfs.push({ &child, drawable.modelMatrix });
		}
	}
	return;
}
