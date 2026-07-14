#include "pass/OpaquePass.h"
#include "core/Logger.h"
#include "render/VulkanBuffer.h"
#include "render/VulkanImage.h"
#include "render/Material.h"

OpaquePass::~OpaquePass() = default;

void OpaquePass::Clear()
{
	if (ctx == nullptr)
		return;
	const VkDevice device = ctx->GetDevice();

	drawables.clear();
	outputImage.reset();
	outputImageDepth.reset();

	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}
	APass::Clear();
}

void OpaquePass::Record(const VulkanContext& ctx, const FrameContext& frame)
{
	const VkCommandBuffer cmd = GetCommandBuffer();
	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VkStructureType::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.imageView = outputImage->GetView();
	colorAttachment.clearValue.color = { {0.f, 0.f, 0.f, 0.f} };
	colorAttachment.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingAttachmentInfo depthAttachment{};
	depthAttachment.sType = VkStructureType::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachment.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachment.imageView = outputImageDepth->GetView();
	depthAttachment.clearValue.depthStencil = { 1.f, 0 };
	depthAttachment.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;
	renderingInfo.pDepthAttachment = &depthAttachment;
	renderingInfo.renderArea = { { 0, 0 }, { outputImage->GetInfo().extent.width, outputImage->GetInfo().extent.height } };
	renderingInfo.layerCount = 1;

	const float x = 0.f;
	const float y = 0.f;
	const float w = static_cast<float>(outputImage->GetInfo().extent.width);
	const float h = static_cast<float>(outputImage->GetInfo().extent.height);

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
	vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, opaqueShader->GetPipelineLayout(), 0, 1, &frame.cameraSet, 0, nullptr);
	for (const Drawable* drawable : drawables)
	{
		if (drawable->mesh == nullptr)
			continue;
		const Material& mat = *drawable->mat;
		const VkDescriptorSet descSet = mat.GetVkDescriptorSet();
		vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, opaqueShader->GetPipelineLayout(), 1, 1, &descSet, 0, nullptr);

		VkBuffer buffer = drawable->mesh->GetVertexBuffer()->GetBuffer();
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &buffer, &offset);
		vkCmdBindIndexBuffer(cmd, drawable->mesh->GetIndexBuffer()->GetBuffer(), 0, VkIndexType::VK_INDEX_TYPE_UINT32);
		vkCmdPushConstants(cmd, opaqueShader->GetPipelineLayout(), VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &drawable->modelMatrix);
		vkCmdDrawIndexed(cmd, drawable->mesh->GetIndices().size(), 1, 0, 0, 0);
	}
	vkCmdEndRendering(cmd);

	drawables.clear();
}

void OpaquePass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	APass::SetUsages(ctx, frame);
	AddUsage(outputImage->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	AddUsage(
		outputImageDepth->GetImage(), 
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT | VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT, 
		VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	for (const Drawable* drawable : drawables)
	{
		if (drawable == nullptr || drawable->mat == nullptr)
			continue;
		for (auto& [view, usingTex] : drawable->mat->GetUsingTextures())
		{
			if (usingTex.imagePtr == nullptr)
				continue;
			VkImageAspectFlags aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
			if (usingTex.imagePtr->GetInfo().format == VkFormat::VK_FORMAT_D32_SFLOAT ||
				usingTex.imagePtr->GetInfo().format == VkFormat::VK_FORMAT_D24_UNORM_S8_UINT)
				aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
			AddUsage(usingTex.imagePtr->GetImage(), aspect, usingTex.usage);
		}
	}
}

void OpaquePass::PushDrawable(const Drawable& drawable)
{
	if (drawable.mat == nullptr || &drawable.mat->shader != opaqueShader)
		return;

	drawables.push_back(&drawable);
}

void OpaquePass::PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout)
{
	VkImageCreateInfo imgCi = VulkanImage::GetCreateInfo();
	imgCi.extent = { 1024, 768, 1 };
	outputImage = std::make_unique<VulkanImage>(ctx, imgCi, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	imgCi.format = VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;
	imgCi.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	outputImageDepth = std::make_unique<VulkanImage>(ctx, imgCi, VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void OpaquePass::BuildPipeline(const VulkanContext& ctx)
{
	const VkDevice device = ctx.GetDevice();

	VkPushConstantRange pc{};
	pc.size = sizeof(glm::mat4);
	pc.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;

	const VkVertexInputBindingDescription vIB = GLBVertex::GetVertexInputBindingDescription();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexAttributeDescriptionCount = GLBVertex::GetVertexInputAttributeDescription().size();
	vertexInputInfo.pVertexAttributeDescriptions = GLBVertex::GetVertexInputAttributeDescription().data();
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &vIB;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	const VkDynamicState dynamicStates[] =
	{
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
	rasterizer.cullMode = VkCullModeFlagBits::VK_CULL_MODE_BACK_BIT;

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

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VkLogicOp::VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
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

	const VkFormat formats[] = { VkFormat::VK_FORMAT_R8G8B8A8_UNORM };
	VkPipelineRenderingCreateInfoKHR pipelineRenderingCI{};
	pipelineRenderingCI.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	pipelineRenderingCI.colorAttachmentCount = 1;
	pipelineRenderingCI.pColorAttachmentFormats = formats;
	pipelineRenderingCI.depthAttachmentFormat = VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;
	pipelineRenderingCI.stencilAttachmentFormat = pipelineRenderingCI.depthAttachmentFormat;

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = opaqueShader->GetPipelineShaderStageCreateInfos().size();
	pipelineInfo.pStages = opaqueShader->GetPipelineShaderStageCreateInfos().data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = opaqueShader->GetPipelineLayout();
	pipelineInfo.renderPass = VK_NULL_HANDLE;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = nullptr;
	pipelineInfo.basePipelineIndex = -1;
	pipelineInfo.pNext = &pipelineRenderingCI;
	VK_RESULT_CHECK(vkCreateGraphicsPipelines(ctx.GetDevice(), nullptr, 1, &pipelineInfo, nullptr, &pipeline));
}
