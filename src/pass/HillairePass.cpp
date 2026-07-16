#include "pass/HillairePass.h"
#include "core/Logger.h"
#include "render/VulkanBuffer.h"
#include "render/VulkanImage.h"
#include "render/Material.h"

void HillairePass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	AtmospherePass::SetUsages(ctx, frame);
	AddUsage(
		transmittanceLUT->GetImage(),
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

auto HillairePass::CreateShader(VkDevice device, VkDescriptorSetLayout cameraSetLayout) -> Shader
{
	std::vector<VkDescriptorSetLayoutBinding> set1Bindings;
	set1Bindings.reserve(5);
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
		AddBinding(2, *opaqueDepthTex, opaqueSampler.GetSampler()).
		AddBinding(3, *opaqueTex, opaqueSampler.GetSampler()).
		AddBinding(4, *shadowMap, shadowSampler->GetSampler()).
		AddBinding(5, *transmittanceLUT, transmittanceLUTSampler->GetSampler()).
		Build(descPool);

	material->UpdateBindingData(0, atmosphere);
}