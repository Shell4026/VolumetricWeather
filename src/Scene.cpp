#include "Scene.h"
#include "Logger.h"
#include "VulkanBuffer.h"

#include "imgui/backends/imgui_impl_vulkan.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

#include <fstream>
#include <vector>
#include <cstdint>
#include <array>
Scene::Scene(VulkanContext& ctx, const ImGUI& imgui) :
	ctx(ctx), imgui(imgui)
{
}
void Scene::Init()
{
	camera.SetPos({ 0.f, 0.f, 0.f });
	camera.SetTo({ 0.f, 0.f, -1.f });

	camera.UpdateMatrix();
	cameraUniformData.pos = camera.GetPos();
	cameraUniformData.view = camera.GetMatrixView();
	cameraUniformData.proj = camera.GetMatrixProj();
	uniformData.color = glm::vec4{ 0.f, 1.f, 0.f, 1.f };

	CreateSyncObjects();
	PrepareCommandBuffer();

	PrepareResource();
	PrepareUniformBuffer();
	SetupDescriptorPool();
	SetupDescriptor();

	CreatePipeline();

	BuildComputeCommandBuffer();
}

void Scene::Clear()
{
	vkDeviceWaitIdle(ctx.GetDevice());

	atmosphere.Clear(ctx);

	plane.Clear();
	vkDestroyDescriptorSetLayout(ctx.GetDevice(), descSetLayout, nullptr);
	descSetLayout = VK_NULL_HANDLE;
	vkDestroyDescriptorPool(ctx.GetDevice(), descPool, nullptr);
	descPool = VK_NULL_HANDLE;

	vkDestroySemaphore(ctx.GetDevice(), imageAvailableSemaphore, nullptr);
	imageAvailableSemaphore = VK_NULL_HANDLE;
	for (VkSemaphore semaphore : renderFinishedSemaphores)
	vkDestroySemaphore(ctx.GetDevice(), semaphore, nullptr);
	renderFinishedSemaphores.clear();
	vkDestroyFence(ctx.GetDevice(), inFlightFence, nullptr);
	inFlightFence = VK_NULL_HANDLE;

	vkDestroyPipeline(ctx.GetDevice(), pipeline, nullptr);
	pipeline = VK_NULL_HANDLE;

	vkDestroyShaderModule(ctx.GetDevice(), vertShader, nullptr);
	vertShader = VK_NULL_HANDLE;
	vkDestroyShaderModule(ctx.GetDevice(), fragShader, nullptr);
	fragShader = VK_NULL_HANDLE;

	vkDestroyPipelineLayout(ctx.GetDevice(), pipelineLayout, nullptr);
	pipelineLayout = VK_NULL_HANDLE;
}
void Scene::Atmosphere::Clear(const VulkanContext& ctx)
{
	vkDestroySampler(ctx.GetDevice(), storageImgSampler, nullptr);
	storageImg.reset();
	vkDestroyDescriptorSetLayout(ctx.GetDevice(), descSetLayout, nullptr);
	descSetLayout = VK_NULL_HANDLE;
	vkDestroyPipeline(ctx.GetDevice(), pipeline, nullptr);
	pipeline = VK_NULL_HANDLE;
	vkDestroyShaderModule(ctx.GetDevice(), computeShader, nullptr);
	computeShader = VK_NULL_HANDLE;
	vkDestroyPipelineLayout(ctx.GetDevice(), pipelineLayout, nullptr);
	pipelineLayout = VK_NULL_HANDLE;
}

void Scene::Render(double dt)
{
	static float ydir = 0.f;
	ydir += 30 * dt;
	glm::quat quat{ glm::radians(glm::vec3{0.f, ydir, 0.f}) };
	modelMatrix = glm::mat4_cast(quat) * glm::translate(glm::mat4{ 1.f }, glm::vec3{ 0.f, 0.f, 0.f });

	BeginFrame();
	cameraUniformBuffers->SetData(&cameraUniformData);
	uniformBuffers->SetData(&uniformData);
	BuildCommandBuffer();
	SubmitCommandBuffer();
}

void Scene::CreateSyncObjects()
{
	VkSemaphoreCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VK_RESULT_CHECK(vkCreateSemaphore(ctx.GetDevice(), &ci, nullptr, &imageAvailableSemaphore));
	renderFinishedSemaphores.resize(ctx.GetSwapChainImages().size());
	for (std::size_t i = 0; i < ctx.GetSwapChainImages().size(); ++i)
		VK_RESULT_CHECK(vkCreateSemaphore(ctx.GetDevice(), &ci, nullptr, &renderFinishedSemaphores[i]));

	VkFenceCreateInfo fenceCi{};
	fenceCi.sType = VkStructureType::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCi.flags = VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT;
	VK_RESULT_CHECK(vkCreateFence(ctx.GetDevice(), &fenceCi, nullptr, &inFlightFence));
}

void Scene::CreatePipeline()
{
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(glm::mat4);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	VK_RESULT_CHECK(vkCreatePipelineLayout(ctx.GetDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout));

	vertShader = LoadShader(ctx.GetDevice(), "shaders/mesh.vert.spv");
	fragShader = LoadShader(ctx.GetDevice(), "shaders/mesh.frag.spv");

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShader;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShader;
	fragShaderStageInfo.pName = "main";

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = { vertShaderStageInfo, fragShaderStageInfo };

	VkVertexInputBindingDescription vertexBindingDesc{};
	vertexBindingDesc.binding = 0;
	vertexBindingDesc.stride = sizeof(Vertex);
	vertexBindingDesc.inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;
	VkVertexInputAttributeDescription attrDesc{};
	attrDesc.binding = 0;
	attrDesc.location = 0;
	attrDesc.format = VkFormat::VK_FORMAT_R32G32B32_SFLOAT;
	attrDesc.offset = offsetof(Vertex, v);

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
	vertexInputInfo.vertexAttributeDescriptionCount = 1;
	vertexInputInfo.pVertexAttributeDescriptions = &attrDesc;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkDynamicState dynamicStates[] = {
		VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT,
		VkDynamicState::VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(VkDynamicState);
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VkPolygonMode::VK_POLYGON_MODE_FILL; //채우기
	rasterizer.lineWidth = 1.0f;
	rasterizer.frontFace = VkFrontFace::VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;
	rasterizer.cullMode = VkCullModeFlagBits::VK_CULL_MODE_NONE;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask =
		VkColorComponentFlagBits::VK_COLOR_COMPONENT_R_BIT |
		VkColorComponentFlagBits::VK_COLOR_COMPONENT_G_BIT |
		VkColorComponentFlagBits::VK_COLOR_COMPONENT_B_BIT |
		VkColorComponentFlagBits::VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;

	//finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
	//finalColor.a = newAlpha.a;
	colorBlendAttachment.srcColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VkBlendOp::VK_BLEND_OP_ADD;
	colorBlendAttachment.dstColorBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

	colorBlendAttachment.srcAlphaBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.alphaBlendOp = VkBlendOp::VK_BLEND_OP_ADD;
	colorBlendAttachment.dstAlphaBlendFactor = VkBlendFactor::VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

	VkPipelineColorBlendAttachmentState colorBlendAttachments[] =
	{
		colorBlendAttachment
	};

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VkLogicOp::VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = sizeof(colorBlendAttachments) / sizeof(VkPipelineColorBlendAttachmentState);
	colorBlending.pAttachments = colorBlendAttachments;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkStencilOpState stencilState;
	stencilState.reference = 0;
	stencilState.compareMask = 0xFF;
	stencilState.compareOp = VkCompareOp::VK_COMPARE_OP_ALWAYS;
	stencilState.passOp = VkStencilOp::VK_STENCIL_OP_KEEP;
	stencilState.failOp = VkStencilOp::VK_STENCIL_OP_KEEP;
	stencilState.depthFailOp = VkStencilOp::VK_STENCIL_OP_KEEP;
	stencilState.writeMask = 0xFF;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = true;
	depthStencil.depthWriteEnable = true;
	depthStencil.depthCompareOp = VkCompareOp::VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = false;
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 1.0f;
	depthStencil.stencilTestEnable = true;
	depthStencil.front = stencilState;
	depthStencil.back = stencilState;

	VkFormat formats[] =
	{
		 ctx.GetSwapChainImagesFormat()
	};

	VkPipelineRenderingCreateInfoKHR pipelineRenderingCI{};
	pipelineRenderingCI.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	pipelineRenderingCI.colorAttachmentCount = 1;
	pipelineRenderingCI.pColorAttachmentFormats = formats;
	pipelineRenderingCI.depthAttachmentFormat = VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;
	pipelineRenderingCI.stencilAttachmentFormat = VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = VK_NULL_HANDLE;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = nullptr;
	pipelineInfo.basePipelineIndex = -1;
	pipelineInfo.pNext = &pipelineRenderingCI;

	VK_RESULT_CHECK(vkCreateGraphicsPipelines(ctx.GetDevice(), nullptr, 1, &pipelineInfo, nullptr, &pipeline));

	CreateAtmospherePipeline();
}

void Scene::PrepareCommandBuffer()
{
	VkCommandBufferAllocateInfo info{};
	info.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.commandPool = ctx.GetCommandPool();
	info.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	info.commandBufferCount = 1;
	VK_RESULT_CHECK(vkAllocateCommandBuffers(ctx.GetDevice(), &info, &cmd));

	// 컴퓨트용
	info.commandPool = ctx.GetComputeCommandPool();
	VK_RESULT_CHECK(vkAllocateCommandBuffers(ctx.GetDevice(), &info, &compute.cmd));
}

void Scene::BuildCommandBuffer()
{
	BuildComputeCommandBuffer();

	vkResetCommandBuffer(cmd, 0);

	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VkStructureType::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.imageView = ctx.GetSwapChainImageViews()[currentImgIdx];
	colorAttachment.clearValue.color = { {0.f, 0.f, 0.f, 1.f} };
	colorAttachment.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingAttachmentInfo depthAttachment{};
	depthAttachment.sType = VkStructureType::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachment.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachment.imageView = ctx.GetSwapChainDepthImageViews()[currentImgIdx];
	depthAttachment.clearValue.depthStencil = { 1.f, 0 };
	depthAttachment.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;
	renderingInfo.pDepthAttachment = &depthAttachment;
	renderingInfo.pStencilAttachment = &depthAttachment;
	renderingInfo.renderArea = { { 0, 0 }, { ctx.GetSwapChainExtent().width, ctx.GetSwapChainExtent().height } };
	renderingInfo.layerCount = 1;

	const float x = 0.f;
	const float y = 0.f;
	const float w = ctx.GetSwapChainExtent().width;
	const float h = ctx.GetSwapChainExtent().height;

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	vkBeginCommandBuffer(cmd, &beginInfo);

	ctx.BarrierCommand(cmd, ctx.GetSwapChainImages()[currentImgIdx], VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0,
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

	ctx.BarrierCommand(cmd, ctx.GetSwapChainDepthImages()[currentImgIdx], VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT | VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VkPipelineStageFlagBits::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VkPipelineStageFlagBits::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		0,
		VkAccessFlagBits::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
	//ctx.BarrierCommand(cmd, atmosphere.storageImg[currentFrame], VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
	//	VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
	//	VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	//	VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	//	VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
	//	VkAccessFlagBits::VK_ACCESS_SHADER_WRITE_BIT,
	//	VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);

	vkCmdBeginRendering(cmd, &renderingInfo);

	VkViewport viewport{};
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	viewport.x = x;
	viewport.y = h - y;
	viewport.width = w;
	viewport.height = -h;

	VkRect2D rect{};
	rect.offset.x = x;
	rect.offset.y = y;
	rect.extent.width = w;
	rect.extent.height = h;

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &rect);

	vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descSets, 0, nullptr);

	VkBuffer vertBuffers[] = { plane.GetVertexBuffer()->GetBuffer() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, vertBuffers, offsets);
	vkCmdBindIndexBuffer(cmd, plane.GetIndexBuffer()->GetBuffer(), 0, VkIndexType::VK_INDEX_TYPE_UINT32);
	vkCmdPushConstants(cmd, pipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &modelMatrix);
	vkCmdDrawIndexed(cmd, plane.GetIndices().size(), 1, 0, 0, 0);

	ImDrawData* drawData = ImGui::GetDrawData();
	if (drawData != nullptr)
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);

	ctx.BarrierCommand(cmd, ctx.GetSwapChainImages()[currentImgIdx], VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VkImageLayout::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		0);

	vkEndCommandBuffer(cmd);
}

auto Scene::BeginFrame() -> bool
{
	vkWaitForFences(ctx.GetDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);
	if (!ctx.AcquireNextImage(imageAvailableSemaphore, nullptr, &currentImgIdx))
		return false;
	vkResetFences(ctx.GetDevice(), 1, &inFlightFence);
	return true;
}

void Scene::SubmitCommandBuffer()
{
	{
		VkSubmitInfo info{};
		info.sType = VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &compute.cmd;
		VK_RESULT_CHECK(vkQueueSubmit(ctx.GetGraphicsQueue(), 1, &info, nullptr));
	}
	{
		VkPipelineStageFlags waitStages[] = { VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSubmitInfo info{};
		info.sType = VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &cmd;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &imageAvailableSemaphore;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &renderFinishedSemaphores[currentImgIdx];
		info.pWaitDstStageMask = waitStages;
		VK_RESULT_CHECK(vkQueueSubmit(ctx.GetGraphicsQueue(), 1, &info, inFlightFence));
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
}

void Scene::PrepareUniformBuffer()
{
	const VkMemoryPropertyFlags memProp = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	uniformBuffers = std::make_unique<VulkanBuffer>(VulkanBuffer::Create(ctx, VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, memProp, sizeof(cameraUniformData)));
	uniformBuffers->Map();
	uniformBuffers->SetData(&uniformData);

	cameraUniformBuffers = std::make_unique<VulkanBuffer>(VulkanBuffer::Create(ctx, VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, memProp, sizeof(cameraUniformData)));
	cameraUniformBuffers->Map();
	cameraUniformBuffers->SetData(&cameraUniformData);
}

void Scene::SetupDescriptorPool()
{
	std::vector<VkDescriptorPoolSize> poolSizes;
	VkDescriptorPoolSize& uniformBufferPoolSize = poolSizes.emplace_back();
	uniformBufferPoolSize.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniformBufferPoolSize.descriptorCount = VulkanContext::MAX_CONCURRENT_FRAMES;
	VkDescriptorPoolSize& storageImagePoolSize = poolSizes.emplace_back();
	storageImagePoolSize.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	storageImagePoolSize.descriptorCount = VulkanContext::MAX_CONCURRENT_FRAMES;

	VkDescriptorPoolCreateInfo poolCi{};
	poolCi.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCi.maxSets = VulkanContext::MAX_CONCURRENT_FRAMES * 2;
	poolCi.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolCi.pPoolSizes = poolSizes.data();
	VK_RESULT_CHECK(vkCreateDescriptorPool(ctx.GetDevice(), &poolCi, nullptr, &descPool));
}

void Scene::SetupDescriptor()
{
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
	VkDescriptorSetLayoutBinding& binding0 = setLayoutBindings.emplace_back();
	binding0.binding = 0;
	binding0.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;
	binding0.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding0.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding1 = setLayoutBindings.emplace_back();
	binding1.binding = 1;
	binding1.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
	binding1.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding1.descriptorCount = 1;

	VkDescriptorSetLayoutCreateInfo layoutCi{};
	layoutCi.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCi.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
	layoutCi.pBindings = setLayoutBindings.data();
	VK_RESULT_CHECK(vkCreateDescriptorSetLayout(ctx.GetDevice(), &layoutCi, nullptr, &descSetLayout));

	VkDescriptorSetAllocateInfo descSetAllocInfo{};
	descSetAllocInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descSetAllocInfo.descriptorPool = descPool;
	descSetAllocInfo.pSetLayouts = &descSetLayout;
	descSetAllocInfo.descriptorSetCount = 1;
	VK_RESULT_CHECK(vkAllocateDescriptorSets(ctx.GetDevice(), &descSetAllocInfo, &descSets));

	VkWriteDescriptorSet writeSet{};
	writeSet.sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.dstSet = descSets;
	writeSet.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeSet.descriptorCount = 1;
	writeSet.dstBinding = 0;
	writeSet.pBufferInfo = &cameraUniformBuffers->GetDescriptorBufferInfo();
	vkUpdateDescriptorSets(ctx.GetDevice(), 1, &writeSet, 0, nullptr);
	writeSet.dstBinding = 1;
	writeSet.pBufferInfo = &uniformBuffers->GetDescriptorBufferInfo();
	vkUpdateDescriptorSets(ctx.GetDevice(), 1, &writeSet, 0, nullptr);

	SetupAtmosphereDescriptor();
}

void Scene::PrepareResource()
{
	//plane
	{
		std::vector<Vertex> verts
		{
			{ { -0.5f, 0.5f, 0.f } },
			{ { 0.5f, 0.5f, 0.f } },
			{ { 0.5f, -0.5f, 0.f } },
			{ { -0.5f, -0.5f, 0.f} },
		};
		std::vector<uint32_t> indices
		{
			0, 1, 2, 2, 3, 0
		};
		plane.Init(std::move(verts), std::move(indices));
		plane.CreateBuffer(ctx);
	}
	// 이미지
	{
		VkImageCreateInfo imgCi{};
		imgCi.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imgCi.format = VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
		imgCi.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
		imgCi.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
		imgCi.imageType = VkImageType::VK_IMAGE_TYPE_2D;
		imgCi.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT;
		imgCi.extent = { 1024, 768, 1 };
		imgCi.mipLevels = 1;
		imgCi.arrayLayers = 1;
		imgCi.tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL;
		imgCi.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
		atmosphere.storageImg = 
			std::make_unique<VulkanImage>(VulkanImage::Create(ctx, imgCi, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

		VkSamplerCreateInfo samplerCi{};
		samplerCi.sType = VkStructureType::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCi.magFilter = VkFilter::VK_FILTER_LINEAR;
		samplerCi.minFilter = VkFilter::VK_FILTER_LINEAR;
		samplerCi.mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCi.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCi.addressModeV = samplerCi.addressModeU;
		samplerCi.addressModeW = samplerCi.addressModeU;
		samplerCi.mipLodBias = 0.0f;
		samplerCi.maxAnisotropy = 1.0f;
		samplerCi.compareOp = VkCompareOp::VK_COMPARE_OP_NEVER;
		samplerCi.minLod = 0.0f;
		samplerCi.maxLod = 1.0f;
		samplerCi.borderColor = VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		VK_RESULT_CHECK(vkCreateSampler(ctx.GetDevice(), &samplerCi, nullptr, &atmosphere.storageImgSampler));

		atmosphere.storageImgDescInfo.sampler = atmosphere.storageImgSampler;
		atmosphere.storageImgDescInfo.imageView = atmosphere.storageImg->GetView();
		atmosphere.storageImgDescInfo.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL;
	}
}

auto Scene::LoadShader(VkDevice device, const std::filesystem::path& path) -> VkShaderModule
{
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		SH_ERROR_FORMAT("Failed to read shader: {}", path.string());
		return VK_NULL_HANDLE;
	}
	const std::size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	VkShaderModuleCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = buffer.size();
	ci.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

	VkShaderModule shader = VK_NULL_HANDLE;
	if (VkResult result = vkCreateShaderModule(device, &ci, nullptr, &shader); result != VkResult::VK_SUCCESS) 
	{
		SH_ERROR_FORMAT("Failed to create shader module!: {}", string_VkResult(result));
		throw std::runtime_error("Failed to create shader module!");
	}
	return shader;
}

void Scene::SetupAtmosphereDescriptor()
{
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
	VkDescriptorSetLayoutBinding& binding0 = setLayoutBindings.emplace_back();
	binding0.binding = 0;
	binding0.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	binding0.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binding0.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding1 = setLayoutBindings.emplace_back();
	binding1.binding = 1;
	binding1.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	binding1.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding1.descriptorCount = 1;

	VkDescriptorSetLayoutCreateInfo layoutCi{};
	layoutCi.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCi.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
	layoutCi.pBindings = setLayoutBindings.data();
	VK_RESULT_CHECK(vkCreateDescriptorSetLayout(ctx.GetDevice(), &layoutCi, nullptr, &atmosphere.descSetLayout));

	VkDescriptorSetAllocateInfo descSetAllocInfo{};
	descSetAllocInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descSetAllocInfo.descriptorPool = descPool;
	descSetAllocInfo.pSetLayouts = &atmosphere.descSetLayout;
	descSetAllocInfo.descriptorSetCount = 1;
	VK_RESULT_CHECK(vkAllocateDescriptorSets(ctx.GetDevice(), &descSetAllocInfo, &atmosphere.descSets));

	VkWriteDescriptorSet writeSet{};
	writeSet.sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.dstSet = atmosphere.descSets;
	writeSet.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeSet.descriptorCount = 1;
	writeSet.dstBinding = 0;
	writeSet.pImageInfo = &atmosphere.storageImgDescInfo;
	vkUpdateDescriptorSets(ctx.GetDevice(), 1, &writeSet, 0, nullptr);
	writeSet.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeSet.dstBinding = 1;
	writeSet.pBufferInfo = &cameraUniformBuffers->GetDescriptorBufferInfo();
	writeSet.pImageInfo = nullptr;
	vkUpdateDescriptorSets(ctx.GetDevice(), 1, &writeSet, 0, nullptr);
}

void Scene::CreateAtmospherePipeline()
{
	atmosphere.computeShader = LoadShader(ctx.GetDevice(), "shaders/atmosphere.comp.spv");

	VkPipelineShaderStageCreateInfo shaderStageCI{};
	shaderStageCI.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCI.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	shaderStageCI.module = atmosphere.computeShader;
	shaderStageCI.pName = "main";

	VkPipelineLayoutCreateInfo pipelineLayoutCI{};
	pipelineLayoutCI.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.setLayoutCount = 1;
	pipelineLayoutCI.pSetLayouts = &atmosphere.descSetLayout;
	VK_RESULT_CHECK(vkCreatePipelineLayout(ctx.GetDevice(), &pipelineLayoutCI, nullptr, &atmosphere.pipelineLayout));

	VkComputePipelineCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.layout = atmosphere.pipelineLayout;
	ci.stage = shaderStageCI;
	VK_RESULT_CHECK(vkCreateComputePipelines(ctx.GetDevice(), nullptr, 1, &ci, nullptr, &atmosphere.pipeline));
}

void Scene::BuildComputeCommandBuffer()
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(compute.cmd, &beginInfo);

	ctx.BarrierCommand(compute.cmd, atmosphere.storageImg->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		VkAccessFlagBits::VK_ACCESS_SHADER_WRITE_BIT
	);

	vkCmdBindPipeline(compute.cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, atmosphere.pipeline);
	vkCmdBindDescriptorSets(compute.cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, atmosphere.pipelineLayout, 0, 1, &atmosphere.descSets, 0, nullptr);
	const uint32_t width = atmosphere.storageImg->GetInfo().extent.width;
	const uint32_t height = atmosphere.storageImg->GetInfo().extent.height;
	vkCmdDispatch(compute.cmd, static_cast<uint32_t>(std::ceil(width / 16.f)), static_cast<uint32_t>(std::ceil(height / 16.f)), 1);
	vkEndCommandBuffer(compute.cmd);
}
