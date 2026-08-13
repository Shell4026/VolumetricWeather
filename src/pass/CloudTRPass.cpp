#include "pass/CloudTRPass.h"
#include "pass/CloudPass.h"
#include "Camera.h"

#include "core/Logger.h"
#include "core/Util.h"

#include "render/Shader.h"
#include "render/Material.h"

#include <algorithm>
#include <filesystem>

CloudTRPass::CloudTRPass(const CloudPass& cloudPass) :
	cloudPass(cloudPass)
{
}

void CloudTRPass::Clear()
{
	material.reset();
	shader.reset();
	output.reset();
	output2.reset();
	depth.reset();
	InvalidateHistory();

	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(ctx->GetDevice(), pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}
}
void CloudTRPass::Record(const VulkanContext& ctx, const FrameContext& frame)
{
	material->UpdateBindingData(0, setting);
	setting.pos = frame.cameraPtr->GetPos();
	setting.viewProj = frame.cameraPtr->GetMatrixProj() * frame.cameraPtr->GetMatrixView();
	if (setting.historyValid == 0)
		setting.historyValid = 1;

	const VkCommandBuffer cmd = GetCommandBuffer();
	vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	const uint32_t width = curOutput->GetInfo().extent.width;
	const uint32_t height = curOutput->GetInfo().extent.height;
	const std::array<VkDescriptorSet, 2> descSets = { frame.cameraSet, material->GetVkDescriptorSet() };
	vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, shader->GetPipelineLayout(), 0, descSets.size(), descSets.data(), 0, nullptr);
	vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 16.f)), static_cast<uint32_t>(std::ceil(height / 16.f)), 1.f);
}

void CloudTRPass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	APass::SetUsages(ctx, frame);
	if (cloudSettingRevision != cloudPass.GetSettingRevision())
	{
		InvalidateHistory();
		cloudSettingRevision = cloudPass.GetSettingRevision();
	}
	const VulkanImage* temp = curOutput;
	curOutput = prevOutput;
	prevOutput = temp;

	material->UpdateBindingData(1, *curOutput, nullptr);
	material->UpdateBindingData(2, *depth, nullptr);
	material->UpdateBindingData(5, *prevOutput, sampler->GetSampler());

	AddUsage(curOutput->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
	AddUsage(depth->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
	AddUsage(cloudPass.GetOutputImage()->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(cloudPass.GetDepthImage()->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(prevOutput->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(accum->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
}

void CloudTRPass::SetSetting(const Setting& setting)
{
	this->setting = setting;
}

void CloudTRPass::InvalidateHistory()
{
	setting.historyValid = 0;
}

void CloudTRPass::SetHistoryValid(uint32_t valid)
{
	setting.historyValid = valid;
}

void CloudTRPass::PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout)
{
	VkImageCreateInfo ci = VulkanImage::GetCreateInfo();
	ci.extent = { ctx.GetSwapChainExtent().width / 2, ctx.GetSwapChainExtent().height / 2, 1};
	ci.format = VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT;
	ci.imageType = VkImageType::VK_IMAGE_TYPE_2D;
	ci.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT;
	output = std::make_unique<VulkanImage>(ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	output2 = std::make_unique<VulkanImage>(ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ci.format = VkFormat::VK_FORMAT_R32_SFLOAT;
	depth = std::make_unique<VulkanImage>(ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ci.format = VkFormat::VK_FORMAT_R8_UINT;
	accum = std::make_unique<VulkanImage>(ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	CreateTRShader(cameraSetLayout);

	curOutput = output.get();
	prevOutput = output2.get();
	cloudSettingRevision = cloudPass.GetSettingRevision();
	
	sampler = &samplerManager->GetLinearClamp();
}

void CloudTRPass::SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	material = std::make_unique<Material>(ctx, *shader);
	material->
		AddBinding<Setting>(0).
		AddBinding(1, *curOutput).
		AddBinding(2, *depth).
		AddBinding(3, *cloudPass.GetOutputImage(), sampler->GetSampler()).
		AddBinding(4, *cloudPass.GetDepthImage(), sampler->GetSampler()).
		AddBinding(5, *prevOutput, sampler->GetSampler()).
		AddBinding(6, *accum).
		Build(descPool);
	material->UpdateBindingData(0, setting);
}

void CloudTRPass::BuildPipeline(const VulkanContext& ctx)
{
	VkComputePipelineCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.layout = shader->GetPipelineLayout();
	ci.stage = shader->GetPipelineShaderStageCreateInfos().front();
	VK_RESULT_CHECK(vkCreateComputePipelines(ctx.GetDevice(), nullptr, 1, &ci, nullptr, &pipeline));
}

void CloudTRPass::CreateTRShader(VkDescriptorSetLayout cameraSetLayout)
{
	std::vector<VkDescriptorSetLayoutBinding> set1Bindings;
	set1Bindings.reserve(7);
	{
		VkDescriptorSetLayoutBinding& binding = set1Bindings.emplace_back();
		binding.binding = 0;
		binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binding.descriptorCount = 1;
	}
	{
		VkDescriptorSetLayoutBinding& binding = set1Bindings.emplace_back();
		binding.binding = 1;
		binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		binding.descriptorCount = 1;
	}
	{
		VkDescriptorSetLayoutBinding& binding = set1Bindings.emplace_back();
		binding.binding = 2;
		binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		binding.descriptorCount = 1;
	}
	{
		VkDescriptorSetLayoutBinding& binding = set1Bindings.emplace_back();
		binding.binding = 3;
		binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
	}
	{
		VkDescriptorSetLayoutBinding& binding = set1Bindings.emplace_back();
		binding.binding = 4;
		binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
	}
	{
		VkDescriptorSetLayoutBinding& binding = set1Bindings.emplace_back();
		binding.binding = 5;
		binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
	}
	{
		VkDescriptorSetLayoutBinding& binding = set1Bindings.emplace_back();
		binding.binding = 6;
		binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		binding.descriptorCount = 1;
	}
	shader = std::make_unique<Shader>();
	shader->
		AddSet(0, cameraSetLayout).
		AddSet(1, std::move(set1Bindings)).
		Build(ctx->GetDevice(), "shaders/cloudTR.comp.spv");
}
