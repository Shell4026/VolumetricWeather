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

	aerialShadow.material.reset();
	aerialShadow.shader.reset();
	aerialShadow.lut.reset();
	if (aerialShadow.pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, aerialShadow.pipeline, nullptr);
		aerialShadow.pipeline = VK_NULL_HANDLE;
	}

	aerialPerspective.material.reset();
	aerialPerspective.shader.reset();
	aerialPerspective.lut.reset();
	if (aerialPerspective.pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, aerialPerspective.pipeline, nullptr);
		aerialPerspective.pipeline = VK_NULL_HANDLE;
	}
	aerialPerspective.sampler.reset();

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
	if (updateLUTFlags == 0)
		return;
	const VkCommandBuffer cmd = GetCommandBuffer();
	UpdateMaterials();
	// Transmittance LUT
	if (updateLUTFlags & LUTType::Transmittance)
	{
		const uint32_t width = transmittance.lut->GetInfo().extent.width;
		const uint32_t height = transmittance.lut->GetInfo().extent.height;

		vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, transmittance.pipeline);
		std::array<VkDescriptorSet, 1> descSets = { transmittance.material->GetVkDescriptorSet() };
		vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, transmittance.shader->GetPipelineLayout(), 1, descSets.size(), descSets.data(), 0, nullptr);
		vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 16.f)), static_cast<uint32_t>(std::ceil(height / 16.f)), 1);

		updateLUTFlags = LUTType::All; // 투과율 LUT가 바뀌면 사실상 다 갱신해야
	}
	// Sky-View LUT
	if (updateLUTFlags & LUTType::SkyView)
	{
		VulkanContext::BarrierCommand(cmd, transmittance.lut->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
			VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VkAccessFlagBits::VK_ACCESS_SHADER_WRITE_BIT, VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT);

		const uint32_t width = skyView.lut->GetInfo().extent.width;
		const uint32_t height = skyView.lut->GetInfo().extent.height;

		vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, skyView.pipeline);
		std::array<VkDescriptorSet, 2> descSets = { frame.cameraSet, skyView.material->GetVkDescriptorSet() };
		vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, skyView.shader->GetPipelineLayout(), 0, descSets.size(), descSets.data(), 0, nullptr);
		vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 16.f)), static_cast<uint32_t>(std::ceil(height / 16.f)), 1);

		VulkanContext::BarrierCommand(cmd, transmittance.lut->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
			VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
			VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT, VkAccessFlagBits::VK_ACCESS_NONE);
	}
	// AerialPerspective LUT
	if (updateLUTFlags & LUTType::AerialPerspective)
	{
		VulkanContext::BarrierCommand(cmd, transmittance.lut->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
			VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VkAccessFlagBits::VK_ACCESS_SHADER_WRITE_BIT, VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT);
		// Aerial Perspective
		{
			const uint32_t width = aerialPerspective.lut->GetInfo().extent.width;
			const uint32_t height = aerialPerspective.lut->GetInfo().extent.height;

			vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, aerialPerspective.pipeline);
			std::array<VkDescriptorSet, 2> descSets = { frame.cameraSet, aerialPerspective.material->GetVkDescriptorSet() };
			vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, aerialPerspective.shader->GetPipelineLayout(), 0, descSets.size(), descSets.data(), 0, nullptr);
			vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 8.f)), static_cast<uint32_t>(std::ceil(height / 8.f)), 1);
		}
		// Aerial Shadow
		{
			const uint32_t width = aerialShadow.lut->GetInfo().extent.width;
			const uint32_t height = aerialShadow.lut->GetInfo().extent.height;

			vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, aerialShadow.pipeline);
			std::array<VkDescriptorSet, 2> descSets = { frame.cameraSet, aerialShadow.material->GetVkDescriptorSet() };
			vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, aerialShadow.shader->GetPipelineLayout(), 0, descSets.size(), descSets.data(), 0, nullptr);
			vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 16.f)), static_cast<uint32_t>(std::ceil(height / 16.f)), 1);
		}
		VulkanContext::BarrierCommand(cmd, transmittance.lut->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
			VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
			VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT, VkAccessFlagBits::VK_ACCESS_NONE);
	}
	updateLUTFlags = 0;
}

void LUTPass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	APass::SetUsages(ctx, frame);
	if (updateLUTFlags == 0)
		return;
	AddUsage(shadowMap->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(depthTex->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT | VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	//if (updateLUTFlags & 0b0100)
		AddUsage(transmittance.lut->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
	//if (updateLUTFlags & LUTType::SkyView)
		AddUsage(skyView.lut->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
	//if (updateLUTFlags & LUTType::AerialPerspective)
		AddUsage(aerialPerspective.lut->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
		AddUsage(aerialShadow.lut->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
}

void LUTPass::ReCreateSkyViewLUT(uint32_t width, uint32_t height)
{
	VkImageCreateInfo imageCI = VulkanImage::GetCreateInfo();
	imageCI.format = VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT;
	imageCI.extent = { width, height, 1 };
	imageCI.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT;
	skyView.lut = std::make_unique<VulkanImage>(*ctx, imageCI, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	skyView.material->UpdateBindingData(1, *skyView.lut, VK_NULL_HANDLE);
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
		imageCI.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT;
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
		imageCI.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT;
		skyView.lut = std::make_unique<VulkanImage>(ctx, imageCI, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkSamplerCreateInfo samplerCI = VulkanSampler::GetCreateInfo();
		samplerCI.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCI.addressModeV = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = samplerCI.addressModeU;
		samplerCI.borderColor = VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		skyView.sampler = std::make_unique<VulkanSampler>(ctx, samplerCI);
	}
	// AerialPerspective LUT
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
		VkDescriptorSetLayoutBinding& binding3 = set1Bindings.emplace_back();
		binding3.binding = 3;
		binding3.descriptorCount = 1;
		binding3.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding3.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;

		aerialPerspective.shader = std::make_unique<Shader>();
		aerialPerspective.shader->
			AddSet(0, cameraSetLayout).
			AddSet(1, std::move(set1Bindings)).
			Build(device, "shaders/aerialPerspectiveLUT.comp.spv");

		VkImageCreateInfo imageCI = VulkanImage::GetCreateInfo();
		imageCI.format = VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT;
		imageCI.imageType = VkImageType::VK_IMAGE_TYPE_3D;
		imageCI.extent = { 32, 32, 32 };
		imageCI.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT;
		aerialPerspective.lut = std::make_unique<VulkanImage>(ctx, imageCI, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkSamplerCreateInfo samplerCI = VulkanSampler::GetCreateInfo();
		samplerCI.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.borderColor = VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		aerialPerspective.sampler = std::make_unique<VulkanSampler>(ctx, samplerCI);
	}
	// Aerial Shadow
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
		VkDescriptorSetLayoutBinding& binding3 = set1Bindings.emplace_back();
		binding3.binding = 3;
		binding3.descriptorCount = 1;
		binding3.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding3.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		VkDescriptorSetLayoutBinding& binding4 = set1Bindings.emplace_back();
		binding4.binding = 4;
		binding4.descriptorCount = 1;
		binding4.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding4.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;

		aerialShadow.shader = std::make_unique<Shader>();
		aerialShadow.shader->
			AddSet(0, cameraSetLayout).
			AddSet(1, std::move(set1Bindings)).
			Build(device, "shaders/aerialShadow.comp.spv");

		VkImageCreateInfo imageCI = VulkanImage::GetCreateInfo();
		imageCI.format = VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT;
		imageCI.extent = { ctx.GetSwapChainExtent().width / 2, ctx.GetSwapChainExtent().height / 2, 1};
		imageCI.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT;
		aerialShadow.lut = std::make_unique<VulkanImage>(ctx, imageCI, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}
}

void LUTPass::SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	transmittance.material = std::make_unique<Material>(ctx, *transmittance.shader);
	transmittance.material->
		AddBinding<TransmittanceSetting>(0).
		AddBinding(1, *transmittance.lut).
		Build(descPool);

	skyView.material = std::make_unique<Material>(ctx, *skyView.shader);
	skyView.material->
		AddBinding<SkyViewSetting>(0).
		AddBinding(1, *skyView.lut).
		AddBinding(2, *transmittance.lut, transmittance.sampler->GetSampler()).
		Build(descPool);

	aerialPerspective.material = std::make_unique<Material>(ctx, *aerialPerspective.shader);
	aerialPerspective.material->
		AddBinding<AerialPerspectiveSetting>(0).
		AddBinding(1, *aerialPerspective.lut).
		AddBinding(2, *transmittance.lut, transmittance.sampler->GetSampler()).
		AddBinding(3, *shadowMap, shadowSampler->GetSampler()).
		Build(descPool);

	aerialShadow.material = std::make_unique<Material>(ctx, *aerialShadow.shader);
	aerialShadow.material->
		AddBinding<AerialShadowSetting>(0).
		AddBinding(1, *aerialShadow.lut).
		AddBinding(2, *transmittance.lut, transmittance.sampler->GetSampler()).
		AddBinding(3, *shadowMap, shadowSampler->GetSampler()).
		AddBinding(4, *depthTex, shadowSampler->GetSampler()).
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

	ci.layout = aerialPerspective.shader->GetPipelineLayout();
	ci.stage = aerialPerspective.shader->GetPipelineShaderStageCreateInfos().front();
	VK_RESULT_CHECK(vkCreateComputePipelines(ctx.GetDevice(), nullptr, 1, &ci, nullptr, &aerialPerspective.pipeline));

	ci.layout = aerialShadow.shader->GetPipelineLayout();
	ci.stage = aerialShadow.shader->GetPipelineShaderStageCreateInfos().front();
	VK_RESULT_CHECK(vkCreateComputePipelines(ctx.GetDevice(), nullptr, 1, &ci, nullptr, &aerialShadow.pipeline));
}

void LUTPass::UpdateMaterials()
{
	aerialSetting.atmosphereRadius = shadowSetting.atmosphereRadius = skyViewSetting.atmosphereRadius = transmitSetting.atmosphereRadius = globalSetting.atmosphereRadius;
	aerialSetting.groundRadius = shadowSetting.groundRadius = skyViewSetting.groundRadius = transmitSetting.groundRadius = globalSetting.groundRadius;
	aerialSetting.sun = shadowSetting.sun = skyViewSetting.sun = globalSetting.sun;
	aerialSetting.sunViewProj = shadowSetting.sunViewProj = globalSetting.sunViewProj;

	transmitSetting.steps = globalSetting.transmittanceLUTSteps;
	skyViewSetting.steps = globalSetting.skyViewLUTSteps;
	aerialSetting.steps = globalSetting.aerialPerspectiveLUTSteps;
	shadowSetting.steps = globalSetting.aerialShadowSteps;

	transmittance.material->UpdateBindingData(0, transmitSetting);
	skyView.material->UpdateBindingData(0, skyViewSetting);
	aerialPerspective.material->UpdateBindingData(0, aerialSetting);
	aerialShadow.material->UpdateBindingData(0, shadowSetting);
}