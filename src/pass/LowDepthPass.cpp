#include "pass/LowDepthPass.h"

#include "core/Logger.h"

#include "render/Shader.h"
#include "render/Material.h"

void LowDepthPass::Clear()
{
	halfDepth.reset();
	quarterDepth.reset();
	materialHalf.reset();
	materialQuarter.reset();
	shader.reset();
	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(ctx->GetDevice(), pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}
	depthTex = nullptr;
	APass::Clear();
}

void LowDepthPass::SetUsages(const FrameContext& frame)
{
	APass::SetUsages(frame);
	AddUsage(halfDepth->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
	AddUsage(quarterDepth->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
	AddUsage(depthTex->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void LowDepthPass::Record(const FrameContext& frame)
{
	const VkCommandBuffer cmd = GetCommandBuffer();
	vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	{
		const uint32_t width = halfDepth->GetInfo().extent.width;
		const uint32_t height = halfDepth->GetInfo().extent.height;
		const VkDescriptorSet descSet = materialHalf->GetVkDescriptorSet();
		vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, shader->GetPipelineLayout(), 1, 1, &descSet, 0, nullptr);
		vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 16.f)), static_cast<uint32_t>(std::ceil(height / 16.f)), 1.f);
	}
	{
		const uint32_t width = quarterDepth->GetInfo().extent.width;
		const uint32_t height = quarterDepth->GetInfo().extent.height;
		const VkDescriptorSet descSet = materialQuarter->GetVkDescriptorSet();
		vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, shader->GetPipelineLayout(), 1, 1, &descSet, 0, nullptr);
		vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 16.f)), static_cast<uint32_t>(std::ceil(height / 16.f)), 1.f);
	}
}

void LowDepthPass::SetDepthTexture(const VulkanImage& tex)
{
	depthTex = &tex;
	if (materialHalf != nullptr)
		materialHalf->UpdateBindingData(1, *depthTex, depthSampler->GetSampler());
	if (materialQuarter != nullptr)
		materialQuarter->UpdateBindingData(1, *depthTex, depthSampler->GetSampler());
}

void LowDepthPass::PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout)
{
	VkImageCreateInfo ci = VulkanImage::GetCreateInfo();
	ci.extent = { ctx.GetSwapChainExtent().width / 2, ctx.GetSwapChainExtent().height / 2, 1 };
	ci.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT;
	ci.format = VkFormat::VK_FORMAT_R32_SFLOAT;
	halfDepth = std::make_unique<VulkanImage>(ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ci.extent = { ctx.GetSwapChainExtent().width / 4, ctx.GetSwapChainExtent().height / 4, 1 };
	quarterDepth = std::make_unique<VulkanImage>(ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	std::vector<VkDescriptorSetLayoutBinding> set1Bindings;
	set1Bindings.reserve(2);
	{
		VkDescriptorSetLayoutBinding& binding = set1Bindings.emplace_back();
		binding.binding = 0;
		binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		binding.descriptorCount = 1;
	}
	{
		VkDescriptorSetLayoutBinding& binding = set1Bindings.emplace_back();
		binding.binding = 1;
		binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
	}

	shader = std::make_unique<Shader>();
	shader->
		AddSet(0, ctx.GetEmptyDescriptorSetLayout()).
		AddSet(1, std::move(set1Bindings)).
		Build(ctx.GetDevice(), "shaders/DepthDown.comp.spv");

	depthSampler = &samplerManager->GetPointClampWhite();
}

void LowDepthPass::SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	materialHalf = std::make_unique<Material>(ctx, *shader);
	materialHalf->
		AddBinding(0, *halfDepth).
		AddBinding(1, *depthTex, depthSampler->GetSampler()).
		Build(descPool);

	materialQuarter = std::make_unique<Material>(ctx, *shader);
	materialQuarter->
		AddBinding(0, *quarterDepth).
		AddBinding(1, *depthTex, depthSampler->GetSampler()).
		Build(descPool);
}

void LowDepthPass::BuildPipeline(const VulkanContext& ctx)
{
	VkComputePipelineCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.layout = shader->GetPipelineLayout();
	ci.stage = shader->GetPipelineShaderStageCreateInfos().front();
	VK_RESULT_CHECK(vkCreateComputePipelines(ctx.GetDevice(), nullptr, 1, &ci, nullptr, &pipeline));
}