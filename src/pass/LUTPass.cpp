#include "pass/LUTPass.h"

#include "core/Logger.h"

#include "render/VulkanImage.h"
#include "render/Material.h"
#include "render/Shader.h"

#ifdef max
#undef max
#endif
void LUTPass::Clear()
{
	const VkDevice device = ctx->GetDevice();

	skyView.material.reset();
	skyView.shader.reset();
	skyView.lut.reset();
	if (skyView.pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, skyView.pipeline, nullptr);
		skyView.pipeline = VK_NULL_HANDLE;
	}
	skyView.sampler.reset();

	transmittance.material.reset();
	transmittance.shader.reset();
	transmittance.lut.reset();
	if (transmittance.pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, transmittance.pipeline, nullptr);
		transmittance.pipeline = VK_NULL_HANDLE;
	}
	transmittance.sampler.reset();

	APass::Clear();
}

void LUTPass::Record(const VulkanContext& ctx, const FrameContext& frame)
{
	const VkCommandBuffer cmd = GetCommandBuffer();
	// Transmittance LUT
	{
		transmittance.material->UpdateBindingData(0, setting);
		
		uint32_t width = transmittance.lut->GetInfo().extent.width;
		uint32_t height = transmittance.lut->GetInfo().extent.height;

		vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, transmittance.pipeline);
		std::array<VkDescriptorSet, 1> descSets = { transmittance.material->GetVkDescriptorSet() };
		vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, transmittance.shader->GetPipelineLayout(), 1, descSets.size(), descSets.data(), 0, nullptr);
		vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 16.f)), static_cast<uint32_t>(std::ceil(height / 16.f)), 1);
	}
	// Sky-View LUT
	{
		skyView.material->UpdateBindingData(0, skyViewSetting);
		VulkanContext::BarrierCommand(cmd, transmittance.lut->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
			VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VkAccessFlagBits::VK_ACCESS_SHADER_WRITE_BIT, VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT);

		uint32_t width = skyView.lut->GetInfo().extent.width;
		uint32_t height = skyView.lut->GetInfo().extent.height;

		vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, skyView.pipeline);
		std::array<VkDescriptorSet, 2> descSets = { frame.cameraSet, skyView.material->GetVkDescriptorSet() };
		vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, skyView.shader->GetPipelineLayout(), 0, descSets.size(), descSets.data(), 0, nullptr);
		vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 16.f)), static_cast<uint32_t>(std::ceil(height / 16.f)), 1);

		VulkanContext::BarrierCommand(cmd, transmittance.lut->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
			VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
			VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT, VkAccessFlagBits::VK_ACCESS_NONE);
	}
}

void LUTPass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	APass::SetUsages(ctx, frame);
	AddUsage(transmittance.lut->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
	AddUsage(skyView.lut->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
}

void LUTPass::PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout)
{
	const VkDevice device = ctx.GetDevice();

	//Transmittance LUT
	{
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
	// Sky-View LUT
	{
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
		VkDescriptorSetLayoutBinding& binding2 = set1Bindings.emplace_back();
		binding2.binding = 2;
		binding2.descriptorCount = 1;
		binding2.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding2.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;

		skyView.shader = std::make_unique<Shader>();
		skyView.shader->
			AddSet(0, cameraSetLayout).
			AddSet(1, std::move(set1Bindings)).
			Build(device, "shaders/skyView.comp.spv");

		VkImageCreateInfo imageCI = VulkanImage::GetCreateInfo();
		imageCI.format = VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT;
		imageCI.extent = { 192, 168, 1 };
		imageCI.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		skyView.lut = std::make_unique<VulkanImage>(ctx, imageCI, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkSamplerCreateInfo samplerCI = VulkanSampler::GetCreateInfo();
		samplerCI.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCI.addressModeV = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = samplerCI.addressModeU;
		samplerCI.borderColor = VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		skyView.sampler = std::make_unique<VulkanSampler>(ctx, samplerCI);
	}
}

void LUTPass::SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	transmittance.material = std::make_unique<Material>(ctx, *transmittance.shader);
	transmittance.material->
		AddBinding<Setting>(0).
		AddBinding(1, *transmittance.lut).
		Build(descPool);

	skyView.material = std::make_unique<Material>(ctx, *skyView.shader);
	skyView.material->
		AddBinding<SkyViewSetting>(0).
		AddBinding(1, *skyView.lut).
		AddBinding(2, *transmittance.lut, transmittance.sampler->GetSampler()).
		Build(descPool);
}

void LUTPass::BuildPipeline(const VulkanContext& ctx)
{
	VkComputePipelineCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.layout = transmittance.shader->GetPipelineLayout();
	ci.stage = transmittance.shader->GetPipelineShaderStageCreateInfos().front();
	VK_RESULT_CHECK(vkCreateComputePipelines(ctx.GetDevice(), nullptr, 1, &ci, nullptr, &transmittance.pipeline));

	ci.layout = skyView.shader->GetPipelineLayout();
	ci.stage = skyView.shader->GetPipelineShaderStageCreateInfos().front();
	VK_RESULT_CHECK(vkCreateComputePipelines(ctx.GetDevice(), nullptr, 1, &ci, nullptr, &skyView.pipeline));
}
