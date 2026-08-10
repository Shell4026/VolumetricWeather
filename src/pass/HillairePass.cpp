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

void HillairePass::Record(const VulkanContext& ctx, const FrameContext& frame)
{
	material->UpdateBindingData(8, *cloudTRPass.GetOutputImage(), cloudTRPass.GetSampler()->GetSampler());
	material->UpdateBindingData(9, *cloudTRPass.GetDepthImage(), cloudTRPass.GetSampler()->GetSampler());
	AtmospherePass::Record(ctx, frame);
}

void HillairePass::UpdateMaterial()
{
	material->UpdateBindingData(5, *lutPass.GetSkyViewLUT(), lutPass.GetSkyViewLUTSampler()->GetSampler());
	material->UpdateBindingData(6, *lutPass.GetAerialPerspectiveLUT(), lutPass.GetAerialPerspectiveSampler()->GetSampler());
	material->UpdateBindingData(7, *lutPass.GetAerialShadowLUT(), lutPass.GetAerialShadowSampler()->GetSampler());
	material->UpdateBindingData(8, *cloudTRPass.GetOutputImage(), cloudTRPass.GetSampler()->GetSampler());
	material->UpdateBindingData(9, *cloudTRPass.GetDepthImage(), cloudTRPass.GetSampler()->GetSampler());
}

auto HillairePass::CreateShader(VkDevice device, VkDescriptorSetLayout cameraSetLayout) -> Shader
{
	std::vector<VkDescriptorSetLayoutBinding> set1Bindings;
	set1Bindings.reserve(11);
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
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
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
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
	}
	{
		VkDescriptorSetLayoutBinding& binding = set1Bindings.emplace_back();
		binding.binding = 7;
		binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
	}
	{
		VkDescriptorSetLayoutBinding& binding = set1Bindings.emplace_back();
		binding.binding = 8;
		binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
	}
	{
		VkDescriptorSetLayoutBinding& binding = set1Bindings.emplace_back();
		binding.binding = 9;
		binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
	}
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
		AddBinding(5, *lutPass.GetSkyViewLUT(), lutPass.GetSkyViewLUTSampler()->GetSampler()).
		AddBinding(6, *lutPass.GetAerialPerspectiveLUT(), lutPass.GetAerialPerspectiveSampler()->GetSampler()).
		AddBinding(7, *lutPass.GetAerialShadowLUT(), lutPass.GetAerialShadowSampler()->GetSampler()).
		AddBinding(8, *cloudTRPass.GetOutputImage(), cloudTRPass.GetSampler()->GetSampler()).
		AddBinding(9, *cloudTRPass.GetDepthImage(), cloudTRPass.GetSampler()->GetSampler()).
		AddBinding(10, *lowDepthPass.GetHalfDepthTexture(), opaqueDepthSampler->GetSampler()).
		AddBinding(11, *lowDepthPass.GetQuarterDepthTexture(), opaqueDepthSampler->GetSampler()).
		Build(descPool);

	material->UpdateBindingData(0, atmosphere);
}