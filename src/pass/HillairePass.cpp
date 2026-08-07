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

void HillairePass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	AtmospherePass::SetUsages(ctx, frame);
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
		lowDepthPass.GetHalfDepthTexture()->GetImage(),
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(
		lowDepthPass.GetQuarterDepthTexture()->GetImage(),
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void HillairePass::Record(const VulkanContext& ctx, const FrameContext& frame)
{
	material->UpdateBindingData(9, *cloudTRPass.GetOutputImage(), cloudTRPass.GetSampler()->GetSampler());
	AtmospherePass::Record(ctx, frame);
}

void HillairePass::UpdateMaterial()
{
	material->UpdateBindingData(5, *lutPass.GetTransmittanceLUT(), lutPass.GetTransmittanceLUTSampler()->GetSampler());
	material->UpdateBindingData(6, *lutPass.GetSkyViewLUT(), lutPass.GetSkyViewLUTSampler()->GetSampler());
	material->UpdateBindingData(7, *lutPass.GetAerialPerspectiveLUT(), lutPass.GetAerialPerspectiveSampler()->GetSampler());
	material->UpdateBindingData(8, *lutPass.GetAerialShadowLUT(), lutPass.GetAerialShadowSampler()->GetSampler());
	material->UpdateBindingData(9, *cloudTRPass.GetOutputImage(), cloudTRPass.GetSampler()->GetSampler());
}

auto HillairePass::CreateShader(VkDevice device, VkDescriptorSetLayout cameraSetLayout) -> Shader
{
	std::vector<VkDescriptorSetLayoutBinding> set1Bindings;
	set1Bindings.reserve(11);
	VkDescriptorSetLayoutBinding& binding0 = set1Bindings.emplace_back();
	binding0.binding = 0;
	binding0.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	binding0.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding0.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding1 = set1Bindings.emplace_back();
	binding1.binding = 1;
	binding1.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	binding1.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binding1.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding2 = set1Bindings.emplace_back();
	binding2.binding = 2;
	binding2.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	binding2.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding2.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding3 = set1Bindings.emplace_back();
	binding3.binding = 3;
	binding3.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	binding3.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding3.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding4 = set1Bindings.emplace_back();
	binding4.binding = 4;
	binding4.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	binding4.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding4.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding5 = set1Bindings.emplace_back();
	binding5.binding = 5;
	binding5.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	binding5.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding5.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding6 = set1Bindings.emplace_back();
	binding6.binding = 6;
	binding6.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	binding6.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding6.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding7 = set1Bindings.emplace_back();
	binding7.binding = 7;
	binding7.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	binding7.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding7.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding8 = set1Bindings.emplace_back();
	binding8.binding = 8;
	binding8.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	binding8.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding8.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding9 = set1Bindings.emplace_back();
	binding9.binding = 9;
	binding9.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	binding9.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding9.descriptorCount = 1;
	{
		VkDescriptorSetLayoutBinding& binding = set1Bindings.emplace_back();
		binding.binding = 10;
		binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
	}
	{
		VkDescriptorSetLayoutBinding& binding = set1Bindings.emplace_back();
		binding.binding = 11;
		binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
	}

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
		AddBinding<Atmosphere>(0).
		AddBinding(1, *outputImage).
		AddBinding(2, *opaqueDepthTex, opaqueDepthSampler->GetSampler()).
		AddBinding(3, *opaqueTex, opaqueSampler->GetSampler()).
		AddBinding(4, *shadowMap, shadowSampler->GetSampler()).
		AddBinding(5, *lutPass.GetTransmittanceLUT(), lutPass.GetTransmittanceLUTSampler()->GetSampler()).
		AddBinding(6, *lutPass.GetSkyViewLUT(), lutPass.GetSkyViewLUTSampler()->GetSampler()).
		AddBinding(7, *lutPass.GetAerialPerspectiveLUT(), lutPass.GetAerialPerspectiveSampler()->GetSampler()).
		AddBinding(8, *lutPass.GetAerialShadowLUT(), lutPass.GetAerialShadowSampler()->GetSampler()).
		AddBinding(9, *cloudTRPass.GetOutputImage(), cloudTRPass.GetSampler()->GetSampler()).
		AddBinding(10, *lowDepthPass.GetHalfDepthTexture(), opaqueDepthSampler->GetSampler()).
		AddBinding(11, *lowDepthPass.GetQuarterDepthTexture(), opaqueDepthSampler->GetSampler()).
		Build(descPool);

	material->UpdateBindingData(0, atmosphere);
}