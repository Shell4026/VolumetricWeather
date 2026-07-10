#include "Pass/CompositePass.h"
#include "core/Logger.h"
#include "render/VulkanBuffer.h"
#include "render/VulkanImage.h"

#include "imgui/backends/imgui_impl_vulkan.h"

#include <vector>
CompositePass::CompositePass(const VulkanImage& opaqueTex, const VulkanImage& atmosphereTex) :
	opaqueTex(opaqueTex), atmosphereTex(atmosphereTex)
{
}
void CompositePass::Clear(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	APass::Clear(ctx, descPool);
	const VkDevice device = ctx.GetDevice();
	
	if (atmosphereSampler != VK_NULL_HANDLE)
	{
		vkDestroySampler(device, atmosphereSampler, nullptr);
		atmosphereSampler = VK_NULL_HANDLE;
	}

	buffer.reset();

	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}
}
void CompositePass::Record(const VulkanContext& ctx, const FrameContext& frame)
{
	const VkCommandBuffer cmd = GetCommandBuffer();
	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VkStructureType::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.imageView = ctx.GetSwapChainImageViews()[frame.imgIdx];
	colorAttachment.clearValue.color = { {0.f, 0.f, 0.f, 1.f} };
	colorAttachment.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;
	renderingInfo.renderArea = { { 0, 0 }, { ctx.GetSwapChainExtent().width, ctx.GetSwapChainExtent().height } };
	renderingInfo.layerCount = 1;

	const float x = 0.f;
	const float y = 0.f;
	const float w = ctx.GetSwapChainExtent().width;
	const float h = ctx.GetSwapChainExtent().height;

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
	vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, compositeShader.GetPipelineLayout(), 1, 1, &descSet, 0, nullptr);
	vkCmdDraw(cmd, 4, 1, 0, 0);

	ImDrawData* drawData = ImGui::GetDrawData();
	if (drawData != nullptr)
		ImGui_ImplVulkan_RenderDrawData(drawData, cmd);

	vkCmdEndRendering(cmd);
}
void CompositePass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	APass::SetUsages(ctx, frame);
	AddUsage(ctx.GetSwapChainImages()[frame.imgIdx], VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	AddUsage(opaqueTex.GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(atmosphereTex.GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
void CompositePass::PrepareResource(const VulkanContext& ctx)
{
	const VkBufferUsageFlags usage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	const VkMemoryPropertyFlags memProp = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	buffer = std::make_unique<VulkanBuffer>(VulkanBuffer::Create(ctx, usage, memProp, sizeof(glm::vec4), &color));

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
	VK_RESULT_CHECK(vkCreateSampler(ctx.GetDevice(), &samplerCi, nullptr, &atmosphereSampler));
}
void CompositePass::SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool, VkDescriptorSetLayout cameraSetLayout)
{
	const VkDevice device = ctx.GetDevice();

	std::vector<VkDescriptorSetLayoutBinding> set1LayoutBindings;
	VkDescriptorSetLayoutBinding& binding0 = set1LayoutBindings.emplace_back();
	binding0.binding = 0;
	binding0.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
	binding0.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding0.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding1 = set1LayoutBindings.emplace_back();
	binding1.binding = 1;
	binding1.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
	binding1.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding1.descriptorCount = 1;

	std::vector<Shader::SetInfo> shaderInfos(2);
	shaderInfos[1].bindings = std::move(set1LayoutBindings);
	compositeShader.Init(ctx.GetDevice(), shaderInfos);
	compositeShader.LoadShaderModule("shaders/composite.vert.spv", "shaders/composite.frag.spv");

	VkDescriptorSetAllocateInfo descSetAllocInfo{};
	descSetAllocInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descSetAllocInfo.descriptorPool = descPool;
	descSetAllocInfo.pSetLayouts = &compositeShader.GetDescriptorSetLayouts()[1];
	descSetAllocInfo.descriptorSetCount = 1;
	VK_RESULT_CHECK(vkAllocateDescriptorSets(device, &descSetAllocInfo, &descSet));

	VkDescriptorImageInfo descImageInfo{};
	descImageInfo.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descImageInfo.imageView = atmosphereTex.GetView();
	descImageInfo.sampler = atmosphereSampler;
	VkWriteDescriptorSet write{};
	write.sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.dstSet = descSet;
	write.dstBinding = 0;
	write.pImageInfo = &descImageInfo;
	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	descImageInfo.imageView = opaqueTex.GetView();
	write.dstBinding = 1;
	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void CompositePass::BuildPipeline(const VulkanContext& ctx)
{
	const VkDevice device = ctx.GetDevice();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
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
	depthStencil.depthTestEnable = false;
	depthStencil.depthWriteEnable = false;
	depthStencil.depthCompareOp = VkCompareOp::VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = false;
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 1.0f;
	depthStencil.stencilTestEnable = false;
	depthStencil.front = stencilState;
	depthStencil.back = stencilState;

	const VkFormat formats[] = { ctx.GetSwapChainImagesFormat() };
	VkPipelineRenderingCreateInfoKHR pipelineRenderingCI{};
	pipelineRenderingCI.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	pipelineRenderingCI.colorAttachmentCount = 1;
	pipelineRenderingCI.pColorAttachmentFormats = formats;
	pipelineRenderingCI.depthAttachmentFormat = VkFormat::VK_FORMAT_UNDEFINED;
	pipelineRenderingCI.stencilAttachmentFormat = VkFormat::VK_FORMAT_UNDEFINED;

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = compositeShader.GetPipelineShaderStageCreateInfos().size();
	pipelineInfo.pStages = compositeShader.GetPipelineShaderStageCreateInfos().data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = compositeShader.GetPipelineLayout();
	pipelineInfo.renderPass = VK_NULL_HANDLE;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = nullptr;
	pipelineInfo.basePipelineIndex = -1;
	pipelineInfo.pNext = &pipelineRenderingCI;
	VK_RESULT_CHECK(vkCreateGraphicsPipelines(ctx.GetDevice(), nullptr, 1, &pipelineInfo, nullptr, &pipeline));
}