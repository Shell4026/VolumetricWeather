#include "pass/LUTPass.h"

#include "core/Logger.h"

#include "render/VulkanImage.h"
#include "render/Material.h"
#include "render/Shader.h"
void LUTPass::Clear()
{
	const VkDevice device = ctx->GetDevice();

	transmittance.sampler.reset();
	transmittance.material.reset();
	transmittance.shader.reset();
	transmittance.lut.reset();

	if (transmittance.pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, transmittance.pipeline, nullptr);
		transmittance.pipeline = VK_NULL_HANDLE;
	}
	APass::Clear();
}

void LUTPass::Record(const VulkanContext& ctx, const FrameContext& frame)
{
	transmittance.material->UpdateBindingData(0, setting);

	const VkCommandBuffer cmd = GetCommandBuffer();
	const uint32_t width = transmittance.lut->GetInfo().extent.width;
	const uint32_t height = transmittance.lut->GetInfo().extent.height;

	vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, transmittance.pipeline);
	std::array<VkDescriptorSet, 1> descSets = { transmittance.material->GetVkDescriptorSet() };
	vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, transmittance.shader->GetPipelineLayout(), 1, descSets.size(), descSets.data(), 0, nullptr);
	vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 16.f)), static_cast<uint32_t>(std::ceil(height / 16.f)), 1);
}

void LUTPass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	APass::SetUsages(ctx, frame);
	AddUsage(transmittance.lut->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
}

void LUTPass::PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout)
{
	const VkDevice device = ctx.GetDevice();

	std::vector<VkDescriptorSetLayoutBinding> set1Bindings;
	set1Bindings.reserve(2);
	VkDescriptorSetLayoutBinding& binding0 = set1Bindings.emplace_back();
	binding0.binding = 0;
	binding0.descriptorCount = 1;
	binding0.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding0.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	VkDescriptorSetLayoutBinding& binding1 = set1Bindings.emplace_back();
	binding1.binding = 1;
	binding1.descriptorCount = 1;
	binding1.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binding1.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;

	transmittance.shader = std::make_unique<Shader>();
	transmittance.shader->
		AddSet(0, ctx.GetEmptyDescriptorSetLayout()).
		AddSet(1, std::move(set1Bindings)).
		Build(device, "shaders/transmittanceLUT.comp.spv");

	VkImageCreateInfo imageCI = VulkanImage::GetCreateInfo();
	imageCI.format = VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT;
	imageCI.extent = { 256, 256, 1 };
	imageCI.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	transmittance.lut = std::make_unique<VulkanImage>(ctx, imageCI, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkSamplerCreateInfo samplerCI = VulkanSampler::GetCreateInfo();
	samplerCI.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerCI.addressModeV = samplerCI.addressModeU;
	samplerCI.addressModeW = samplerCI.addressModeV;
	samplerCI.borderColor = VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	transmittance.sampler = std::make_unique<VulkanSampler>(ctx, samplerCI);
}

void LUTPass::SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	transmittance.material = std::make_unique<Material>(ctx, *transmittance.shader);
	transmittance.material->
		AddBinding<Setting>(0).
		AddBinding(1, *transmittance.lut).
		Build(descPool);
}

void LUTPass::BuildPipeline(const VulkanContext& ctx)
{
	VkComputePipelineCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.layout = transmittance.shader->GetPipelineLayout();
	ci.stage = transmittance.shader->GetPipelineShaderStageCreateInfos().front();
	VK_RESULT_CHECK(vkCreateComputePipelines(ctx.GetDevice(), nullptr, 1, &ci, nullptr, &transmittance.pipeline));
}
