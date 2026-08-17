#include "pass/HillairePass.h"
#include "pass/LUTPass.h"
#include "pass/cloudTRPass.h"
#include "pass/LowDepthPass.h"

#include "core/Logger.h"

#include "render/VulkanBuffer.h"
#include "render/VulkanImage.h"
#include "render/Material.h"

HillairePass::HillairePass(const LowDepthPass& lowDepthPass, const LUTPass& lutPass, const CloudTRPass& cloudTRPass) :
	lowDepthPass(lowDepthPass), lutPass(lutPass), cloudTRPass(cloudTRPass)
{
}

void HillairePass::SetUsages(const FrameContext& frame)
{
	AtmosphereBasePass::SetUsages(frame);
	AddUsage(
		lutPass.GetTransmittanceLUT()->GetImage(),
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(
		lutPass.GetSkyViewLUT()->GetImage(),
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(
		lutPass.GetAerialPerspectiveLUT()->GetImage(),
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(
		lutPass.GetAerialShadowLUT()->GetImage(),
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(
		cloudTRPass.GetOutputImage()->GetImage(),
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(
		cloudTRPass.GetDepthImage()->GetImage(),
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(
		lowDepthPass.GetHalfDepthTexture()->GetImage(),
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(
		lowDepthPass.GetQuarterDepthTexture()->GetImage(),
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void HillairePass::BeginRecord(const FrameContext& frame, const std::vector<BarrierInfo>* barrierInfos)
{
	AtmosphereBasePass::BeginRecord(frame, barrierInfos);

	material->UpdateBindingData(11, *cloudTRPass.GetOutputImage(), cloudTRPass.GetSampler()->GetSampler());
	material->UpdateBindingData(12, *cloudTRPass.GetDepthImage(), cloudTRPass.GetSampler()->GetSampler());
}

void HillairePass::UpdateMaterial()
{
	material->UpdateBindingData(7, *lutPass.GetTransmittanceLUT(), lutPass.GetTransmittanceLUTSampler()->GetSampler());
	material->UpdateBindingData(8, *lutPass.GetSkyViewLUT(), lutPass.GetSkyViewLUTSampler()->GetSampler());
	material->UpdateBindingData(9, *lutPass.GetAerialPerspectiveLUT(), lutPass.GetAerialPerspectiveSampler()->GetSampler());
	material->UpdateBindingData(10, *lutPass.GetAerialShadowLUT(), lutPass.GetAerialShadowSampler()->GetSampler());
	material->UpdateBindingData(11, *cloudTRPass.GetOutputImage(), cloudTRPass.GetSampler()->GetSampler());
	material->UpdateBindingData(12, *cloudTRPass.GetDepthImage(), cloudTRPass.GetSampler()->GetSampler());
}

void HillairePass::SetSetting(const WeatherSetting::Atmosphere& atmosphereSetting)
{
	material->UpdateBindingData(1, atmosphereSetting);
}

void HillairePass::SetSetting(const WeatherSetting::Lighting & lightingSetting)
{
	material->UpdateBindingData(2, lightingSetting);
}

void HillairePass::SetSetting(const Setting& setting)
{
	this->setting = setting;
	material->UpdateBindingData(0, this->setting);
}

auto HillairePass::CreateShader(VkDevice device, VkDescriptorSetLayout cameraSetLayout) -> Shader
{
	std::vector<VkDescriptorSetLayoutBinding> set1Bindings;
	set1Bindings.reserve(11);
	AddDescSetLayoutBinding(set1Bindings, 0, VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 1, VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 2, VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 3, VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 4, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 5, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 6, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 7, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 8, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 9, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 10, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 11, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 12, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 13, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 14, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);

	Shader shader{};
	shader.
		AddSet(0, cameraSetLayout).
		AddSet(1, std::move(set1Bindings)).
		Build(device, "shaders/atmosphere2.comp.spv");
	return shader;
}

void HillairePass::SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	material = std::make_unique<Material>(ctx, *GetShader());
	material->
		AddBinding<Setting>(0).
		AddBinding<WeatherSetting::Atmosphere>(1).
		AddBinding<WeatherSetting::Lighting>(2).
		AddBinding(3, *outputImage).
		AddBinding(4, *opaqueDepthTex, opaqueDepthSampler->GetSampler()).
		AddBinding(5, *opaqueTex, opaqueSampler->GetSampler()).
		AddBinding(6, *shadowMap, shadowSampler->GetSampler()).
		AddBinding(7, *lutPass.GetTransmittanceLUT(), lutPass.GetTransmittanceLUTSampler()->GetSampler()).
		AddBinding(8, *lutPass.GetSkyViewLUT(), lutPass.GetSkyViewLUTSampler()->GetSampler()).
		AddBinding(9, *lutPass.GetAerialPerspectiveLUT(), lutPass.GetAerialPerspectiveSampler()->GetSampler()).
		AddBinding(10, *lutPass.GetAerialShadowLUT(), lutPass.GetAerialShadowSampler()->GetSampler()).
		AddBinding(11, *cloudTRPass.GetOutputImage(), cloudTRPass.GetSampler()->GetSampler()).
		AddBinding(12, *cloudTRPass.GetDepthImage(), cloudTRPass.GetSampler()->GetSampler()).
		AddBinding(13, *lowDepthPass.GetHalfDepthTexture(), opaqueDepthSampler->GetSampler()).
		AddBinding(14, *lowDepthPass.GetQuarterDepthTexture(), opaqueDepthSampler->GetSampler()).
		Build(descPool);

	material->UpdateBindingData(0, setting);
}
