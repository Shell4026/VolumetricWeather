#include "pass/CloudPaintPass.h"

#include "core/Logger.h"

#include "render/Shader.h"
#include "render/Material.h"

CloudPaintPass::CloudPaintPass() = default;
CloudPaintPass::~CloudPaintPass()
{
	Clear();
}

void CloudPaintPass::Clear()
{
	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(ctx->GetDevice(), pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}
	shader.reset();
	material.reset();

	APass::Clear();
}

void CloudPaintPass::Record(const FrameContext& frame)
{
	const VkCommandBuffer cmd = GetCommandBuffer();

	const uint32_t width = pc.radius * 2 + 1;
	const uint32_t height = pc.radius * 2 + 1;

	vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	VkDescriptorSet descSet = material->GetVkDescriptorSet();
	vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, shader->GetPipelineLayout(), 1, 1, &descSet, 0, nullptr);
	vkCmdPushConstants(cmd, shader->GetPipelineLayout(), VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstant), &pc);
	vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 16.f)), static_cast<uint32_t>(std::ceil(height / 16.f)), 1);
}

void CloudPaintPass::SetUsages(const FrameContext& frame)
{
	APass::SetUsages(frame);
	if (canvasImagePtr == nullptr)
		return;
	AddUsage(canvasImagePtr->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
}

void CloudPaintPass::PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout)
{
	VkPushConstantRange pc{};
	pc.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	pc.offset = 0;
	pc.size = sizeof(PushConstant);


	std::vector<VkDescriptorSetLayoutBinding> set1Bindings;
	AddDescSetLayoutBinding(set1Bindings, 0, VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);

	shader = std::make_unique<Shader>();
	shader->
		AddSet(0, ctx.GetEmptyDescriptorSetLayout()).
		AddSet(1, std::move(set1Bindings)).
		Build(ctx.GetDevice(), "shaders/paint.comp.spv", &pc);
}

void CloudPaintPass::SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	material = std::make_unique<Material>(ctx, *shader);
	material->
		AddBinding(0, *canvasImagePtr).
		Build(descPool);
}

void CloudPaintPass::BuildPipeline(const VulkanContext& ctx)
{
	VkComputePipelineCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.layout = shader->GetPipelineLayout();
	ci.stage = shader->GetPipelineShaderStageCreateInfos().front();
	VK_RESULT_CHECK(vkCreateComputePipelines(ctx.GetDevice(), nullptr, 1, &ci, nullptr, &pipeline));
}
