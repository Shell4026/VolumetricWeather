#include "Scene.h"
#include "Logger.h"
#include "VulkanBuffer.h"

#include <fstream>
#include <vector>
#include <cstdint>
#include <array>
Scene::Scene(VulkanContext& ctx) :
	ctx(ctx)
{
}
void Scene::Init()
{
	uniformData.color = { 1.f, 1.f, 1.f, 1.f };

	CreateSyncObjects();
	PrepareCommandBuffer();
	PrepareUniformBuffer();
	SetupDescriptor();
	CreatePipeline();
}

void Scene::Clear()
{
	vkDeviceWaitIdle(ctx.GetDevice());

	vkDestroyDescriptorSetLayout(ctx.GetDevice(), descSetLayout, nullptr);
	descSetLayout = VK_NULL_HANDLE;
	vkDestroyDescriptorPool(ctx.GetDevice(), descPool, nullptr);
	descPool = VK_NULL_HANDLE;

	for (uint32_t i = 0; i < VulkanContext::MAX_CONCURRENT_FRAMES; ++i)
	{
		vkDestroyFence(ctx.GetDevice(), inFlightFence[i], nullptr);
		inFlightFence[i] = VK_NULL_HANDLE;
		vkDestroySemaphore(ctx.GetDevice(), semaphores[i].imageAvailable, nullptr);
		semaphores[i].imageAvailable = VK_NULL_HANDLE;
	}
	for (VkSemaphore& semaphore : renderFinishedSemaphores)
		vkDestroySemaphore(ctx.GetDevice(), semaphore, nullptr);
	renderFinishedSemaphores.clear();

	vkDestroyPipeline(ctx.GetDevice(), pipeline, nullptr);
	pipeline = VK_NULL_HANDLE;

	vkDestroyShaderModule(ctx.GetDevice(), vertShader, nullptr);
	vertShader = VK_NULL_HANDLE;
	vkDestroyShaderModule(ctx.GetDevice(), fragShader, nullptr);
	fragShader = VK_NULL_HANDLE;

	vkDestroyPipelineLayout(ctx.GetDevice(), pipelineLayout, nullptr);
	pipelineLayout = VK_NULL_HANDLE;
}

void Scene::Render(double dt)
{
	if (!PrepareFrame())
		return;

	static float x = 0.f;
	x += dt;
	float r = (glm::sin(x) + 1) * 0.5f;
	uniformData.color = { r, 1 - r, r, 1.f };
	uniformBuffers[currentFrame]->SetData(&uniformData);

	BuildCommandBuffer();
	SubmitCommandBuffer();
}

void Scene::CreateSyncObjects()
{
	const VkSemaphoreCreateInfo ci
	{
		.sType = VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};

	for (uint32_t i = 0; i < VulkanContext::MAX_CONCURRENT_FRAMES; ++i)
	{
		VK_RESULT_CHECK(vkCreateSemaphore(ctx.GetDevice(), &ci, nullptr, &semaphores[i].imageAvailable));

		const VkFenceCreateInfo fenceCi
		{
			.sType = VkStructureType::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT,
		};
		VK_RESULT_CHECK(vkCreateFence(ctx.GetDevice(), &fenceCi, nullptr, &inFlightFence[i]));
	}
	renderFinishedSemaphores.resize(ctx.GetSwapChainImages().size());
	for (VkSemaphore& semaphore : renderFinishedSemaphores)
		VK_RESULT_CHECK(vkCreateSemaphore(ctx.GetDevice(), &ci, nullptr, &semaphore));
}

void Scene::CreatePipeline()
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	VK_RESULT_CHECK(vkCreatePipelineLayout(ctx.GetDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout));

	vertShader = LoadShader(ctx.GetDevice(), "shaders/triangle.vert.spv");
	fragShader = LoadShader(ctx.GetDevice(), "shaders/triangle.frag.spv");

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

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

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
	rasterizer.polygonMode = VkPolygonMode::VK_POLYGON_MODE_FILL; //Ã¤¿ì±â
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
}

void Scene::PrepareCommandBuffer()
{
	VkCommandBufferAllocateInfo info{};
	info.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.commandPool = ctx.GetCommandPool();
	info.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	info.commandBufferCount = 1;

	for (uint32_t i = 0; i < VulkanContext::MAX_CONCURRENT_FRAMES; ++i)
		VK_RESULT_CHECK(vkAllocateCommandBuffers(ctx.GetDevice(), &info, &cmd[i]));
}

void Scene::BuildCommandBuffer()
{
	vkResetCommandBuffer(cmd[currentFrame], 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;

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
	vkBeginCommandBuffer(cmd[currentFrame], &beginInfo);

	ctx.BarrierCommand(cmd[currentFrame], ctx.GetSwapChainImages()[currentImgIdx], VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED, 
		VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		0, 
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

	ctx.BarrierCommand(cmd[currentFrame], ctx.GetSwapChainDepthImages()[currentImgIdx], VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT | VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED, 
		VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VkPipelineStageFlagBits::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VkPipelineStageFlagBits::VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		0, 
		VkAccessFlagBits::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

	vkCmdBeginRendering(cmd[currentFrame], &renderingInfo);

	VkViewport viewport{};
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	viewport.x = x;
	viewport.y = y + h;
	viewport.width = w;
	viewport.height = -h;

	VkRect2D rect{};
	rect.offset.x = x;
	rect.offset.y = y;
	rect.extent.width = w;
	rect.extent.height = h;
	
	vkCmdSetViewport(cmd[currentFrame], 0, 1, &viewport);
	vkCmdSetScissor(cmd[currentFrame], 0, 1, &rect);

	vkCmdBindPipeline(cmd[currentFrame], VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdBindDescriptorSets(cmd[currentFrame], VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descSets[currentFrame], 0, nullptr);
	vkCmdDraw(cmd[currentFrame], 3, 1, 0, 0);

	vkCmdEndRendering(cmd[currentFrame]);

	ctx.BarrierCommand(cmd[currentFrame], ctx.GetSwapChainImages()[currentImgIdx], VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VkImageLayout::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		0);
	vkEndCommandBuffer(cmd[currentFrame]);
}

void Scene::SubmitCommandBuffer()
{
	VkPipelineStageFlags waitStages[] = { VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo info{};
	info.sType = VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
	info.commandBufferCount = 1;
	info.pCommandBuffers = &cmd[currentFrame];
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &semaphores[currentFrame].imageAvailable;
	info.signalSemaphoreCount = 1;
	info.pSignalSemaphores = &renderFinishedSemaphores[currentImgIdx];
	info.pWaitDstStageMask = waitStages;
	if (VkResult result = vkQueueSubmit(ctx.GetGraphicsQueue(), 1, &info, inFlightFence[currentFrame]); result != VkResult::VK_SUCCESS)
	{
		SH_ERROR_FORMAT("Failed to submit draw command buffer!: {}", string_VkResult(result));
		throw std::runtime_error("Failed to submit draw command buffer!");
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
	
	currentFrame = (currentFrame + 1) % VulkanContext::MAX_CONCURRENT_FRAMES;
}

auto Scene::PrepareFrame() -> bool
{
	vkWaitForFences(ctx.GetDevice(), 1, &inFlightFence[currentFrame], VK_TRUE, UINT64_MAX);
	
	if (!ctx.AcquireNextImage(semaphores[currentFrame].imageAvailable, nullptr, &currentImgIdx))
		return false;
	vkResetFences(ctx.GetDevice(), 1, &inFlightFence[currentFrame]);
	return true;
}

void Scene::PrepareUniformBuffer()
{
	for (uint32_t i = 0; i < VulkanContext::MAX_CONCURRENT_FRAMES; ++i)
	{
		const VkMemoryPropertyFlags memProp = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		uniformBuffers[i] = std::make_unique<VulkanBuffer>(VulkanBuffer::Create(ctx, VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, memProp, sizeof(UniformData)));
		uniformBuffers[i]->Map();
		uniformBuffers[i]->SetData(&uniformData);
	}
}

void Scene::SetupDescriptor()
{
	const VkDescriptorPoolSize poolSize
	{
		.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = VulkanContext::MAX_CONCURRENT_FRAMES
	};
	VkDescriptorPoolCreateInfo poolCi{};
	poolCi.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCi.maxSets = VulkanContext::MAX_CONCURRENT_FRAMES;
	poolCi.poolSizeCount = 1;
	poolCi.pPoolSizes = &poolSize;
	VK_RESULT_CHECK(vkCreateDescriptorPool(ctx.GetDevice(), &poolCi, nullptr, &descPool));

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
	VkDescriptorSetLayoutBinding& binding0 = setLayoutBindings.emplace_back();
	binding0.binding = 0;
	binding0.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
	binding0.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding0.descriptorCount = 1;

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
	for (std::size_t i = 0; i < uniformBuffers.size(); ++i)
	{
		VK_RESULT_CHECK(vkAllocateDescriptorSets(ctx.GetDevice(), &descSetAllocInfo, &descSets[i]));

		VkWriteDescriptorSet writeSet{};
		writeSet.sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet.dstSet = descSets[i];
		writeSet.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeSet.dstBinding = 0;
		writeSet.pBufferInfo = &uniformBuffers[i]->GetDescriptorBufferInfo();
		writeSet.descriptorCount = 1;
		vkUpdateDescriptorSets(ctx.GetDevice(), 1, &writeSet, 0, nullptr);
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
