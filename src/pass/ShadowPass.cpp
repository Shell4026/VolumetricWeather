#include "Pass/ShadowPass.h"

#include "core/Logger.h"

#include "render/VulkanImage.h"
#include "render/Material.h"

#include "GLBLoader.h"

#include "glm/glm.hpp"
ShadowPass::ShadowPass()
{
}
ShadowPass::~ShadowPass()
{
	Clear();
}

void ShadowPass::Clear()
{
	if (ctx == nullptr)
		return;
	const VkDevice device = ctx->GetDevice();

	shadowSampler.reset();
	sunShadowMap.reset();

	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}
	material.reset();
	shader.Clear();
	APass::Clear();
}

void ShadowPass::Record(const VulkanContext& ctx, const FrameContext& frame)
{
	uniformData.pos = cam.GetPos();
	uniformData.viewProj = cam.GetMatrixProj() * cam.GetMatrixView();
	material->UpdateBindingData(0, uniformData);

	const VkCommandBuffer cmd = GetCommandBuffer();

	VkRenderingAttachmentInfo depthAttachment{};
	depthAttachment.sType = VkStructureType::VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachment.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachment.imageView = sunShadowMap->GetView();
	depthAttachment.clearValue.depthStencil = { 1.f, 0 };
	depthAttachment.loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.pDepthAttachment = &depthAttachment;
	renderingInfo.renderArea = { { 0, 0 }, { sunShadowMap->GetInfo().extent.width, sunShadowMap->GetInfo().extent.height } };
	renderingInfo.layerCount = 1;

	const float x = 0.f;
	const float y = 0.f;
	const float w = static_cast<float>(sunShadowMap->GetInfo().extent.width);
	const float h = static_cast<float>(sunShadowMap->GetInfo().extent.height);

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

	const VkDescriptorSet sets[] = { emptyDescSet, material->GetVkDescriptorSet() };
	vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, shader.GetPipelineLayout(), 0, 2, sets, 0, nullptr);
	for (const Drawable* drawable : drawables)
	{
		if (drawable->mesh == nullptr)
			continue;
		VkBuffer buffer = drawable->mesh->GetVertexBuffer()->GetBuffer();
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &buffer, &offset);
		vkCmdBindIndexBuffer(cmd, drawable->mesh->GetIndexBuffer()->GetBuffer(), 0, VkIndexType::VK_INDEX_TYPE_UINT32);
		vkCmdPushConstants(cmd, shader.GetPipelineLayout(), VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &drawable->modelMatrix);
		vkCmdDrawIndexed(cmd, drawable->mesh->GetIndices().size(), 1, 0, 0, 0);
	}
	vkCmdEndRendering(cmd);

	drawables.clear();
}

void ShadowPass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	AddUsage(sunShadowMap->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT, VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
}

void ShadowPass::PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout)
{
	const VkDevice device = ctx.GetDevice();

	VkImageCreateInfo imgCI = VulkanImage::GetCreateInfo();
	imgCI.format = VkFormat::VK_FORMAT_D32_SFLOAT;
	imgCI.extent = { 2048, 2048, 1 };
	imgCI.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	sunShadowMap = std::make_unique<VulkanImage>(ctx, imgCI, VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkSamplerCreateInfo samplerCI = VulkanSampler::GetCreateInfo();
	samplerCI.minFilter = VkFilter::VK_FILTER_LINEAR;
	samplerCI.magFilter = VkFilter::VK_FILTER_LINEAR;
	samplerCI.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerCI.addressModeV = samplerCI.addressModeU;
	samplerCI.addressModeW = samplerCI.addressModeU;
	samplerCI.compareEnable = VK_TRUE;
	samplerCI.compareOp = VkCompareOp::VK_COMPARE_OP_LESS_OR_EQUAL;
	samplerCI.borderColor = VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	shadowSampler = std::make_unique<VulkanSampler>();
	shadowSampler->Create(ctx, samplerCI);

	VkPushConstantRange pc{};
	pc.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;
	pc.size = sizeof(glm::mat4);

	std::vector<VkDescriptorSetLayoutBinding> set1(1);
	set1[0].binding = 0;
	set1[0].descriptorCount = 1;
	set1[0].descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	set1[0].stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;
	shader.AddSet(0, ctx.GetEmptyDescriptorSetLayout());
	shader.AddSet(1, std::move(set1));
	shader.Build(device, "shaders/shadow.vert.spv", "", &pc);
}

void ShadowPass::SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	const VkDescriptorSetLayout emptyLayout = ctx.GetEmptyDescriptorSetLayout();
	VkDescriptorSetAllocateInfo info{};
	info.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	info.descriptorPool = descPool;
	info.descriptorSetCount = 1;
	info.pSetLayouts = &emptyLayout;
	VK_RESULT_CHECK(vkAllocateDescriptorSets(ctx.GetDevice(), &info, &emptyDescSet));

	material = std::make_unique<Material>(ctx, shader);
	material->AddBinding<UniformData>(0);
	material->Build(descPool);

	material->UpdateBindingData(0, uniformData);
}

void ShadowPass::BuildPipeline(const VulkanContext& ctx)
{
	const VkDevice device = ctx.GetDevice();

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
	rasterizer.cullMode = VkCullModeFlagBits::VK_CULL_MODE_NONE;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = true;
	depthStencil.depthWriteEnable = true;
	depthStencil.depthCompareOp = VkCompareOp::VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = false;
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 1.0f;
	depthStencil.stencilTestEnable = false;

	VkPipelineRenderingCreateInfoKHR pipelineRenderingCI{};
	pipelineRenderingCI.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	pipelineRenderingCI.colorAttachmentCount = 0;
	pipelineRenderingCI.pColorAttachmentFormats = nullptr;
	pipelineRenderingCI.depthAttachmentFormat = VkFormat::VK_FORMAT_D32_SFLOAT;
	pipelineRenderingCI.stencilAttachmentFormat = VkFormat::VK_FORMAT_UNDEFINED;

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = shader.GetPipelineShaderStageCreateInfos().size();
	pipelineInfo.pStages = shader.GetPipelineShaderStageCreateInfos().data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = nullptr;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = shader.GetPipelineLayout();
	pipelineInfo.renderPass = VK_NULL_HANDLE;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = nullptr;
	pipelineInfo.basePipelineIndex = -1;
	pipelineInfo.pNext = &pipelineRenderingCI;
	VK_RESULT_CHECK(vkCreateGraphicsPipelines(ctx.GetDevice(), nullptr, 1, &pipelineInfo, nullptr, &pipeline));
}
