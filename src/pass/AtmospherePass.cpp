#include "pass/AtmospherePass.h"

#include "core/Logger.h"

#include "render/Shader.h"
#include "render/VulkanBuffer.h"
#include "render/VulkanImage.h"
#include "render/Material.h"
AtmosphereBasePass::~AtmosphereBasePass()
{
	Clear();
}

void AtmosphereBasePass::Clear()
{
	if (ctx == nullptr)
		return;
	const VkDevice device = ctx->GetDevice();

	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}
	opaqueSampler = nullptr;
	computeShader.reset();
	material.reset();

	outputImage.reset();
	APass::Clear();
}

void AtmosphereBasePass::Record(const FrameContext& frame)
{
	const VkCommandBuffer cmd = GetCommandBuffer();
	const uint32_t width = outputImage->GetInfo().extent.width;
	const uint32_t height = outputImage->GetInfo().extent.height;

	vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	std::array<VkDescriptorSet, 2> descSets = { frame.cameraSet, material->GetVkDescriptorSet() };
	vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, computeShader->GetPipelineLayout(), 0, descSets.size(), descSets.data(), 0, nullptr);
	vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 8.f)), static_cast<uint32_t>(std::ceil(height / 8.f)), 1);
}

void AtmosphereBasePass::SetUsages(const FrameContext& frame)
{
	APass::SetUsages(frame);
	AddUsage(outputImage->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
	AddUsage(
		opaqueDepthTex->GetImage(), 
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT, 
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(
		opaqueTex->GetImage(),
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(
		shadowMap->GetImage(),
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT,
		VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void AtmosphereBasePass::PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout)
{
	this->cameraSetLayout = cameraSetLayout;

	VkImageCreateInfo imgCi = VulkanImage::GetCreateInfo();
	imgCi.format = VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT;
	imgCi.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imgCi.extent = { width, height, 1 };
	outputImage = std::make_unique<VulkanImage>(ctx, imgCi, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	opaqueSampler = &samplerManager->GetLinearClampWhite();
	opaqueDepthSampler = &samplerManager->GetPointClampWhite();

	computeShader = std::make_unique<Shader>(CreateShader(ctx.GetDevice(), cameraSetLayout));
}

void AtmosphereBasePass::BuildPipeline(const VulkanContext& ctx)
{
	VkComputePipelineCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.layout = computeShader->GetPipelineLayout();
	ci.stage = computeShader->GetPipelineShaderStageCreateInfos().front();
	VK_RESULT_CHECK(vkCreateComputePipelines(ctx.GetDevice(), nullptr, 1, &ci, nullptr, &pipeline));
}

void AtmospherePass::SetSetting(const WeatherSetting::Atmosphere& atmosphereSetting)
{
	material->UpdateBindingData(1, atmosphereSetting);
}

void AtmospherePass::SetSetting(const WeatherSetting::Lighting & lightingSetting)
{
	material->UpdateBindingData(2, lightingSetting);
}

void AtmospherePass::SetSetting(const Setting& setting)
{
	this->setting = setting;
	material->UpdateBindingData(0, this->setting);
}

auto AtmospherePass::CreateShader(VkDevice device, VkDescriptorSetLayout cameraSetLayout) -> Shader
{
	std::vector<VkDescriptorSetLayoutBinding> set1Bindings;
	set1Bindings.reserve(7);

	AddDescSetLayoutBinding(set1Bindings, 0, VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 1, VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 2, VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 3, VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 4, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 5, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 6, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);

	Shader shader{};
	shader.
		AddSet(0, cameraSetLayout).
		AddSet(1, std::move(set1Bindings)).
		Build(device, "shaders/atmosphere.comp.spv");
	return shader;
}

void AtmospherePass::SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	const VkDevice device = ctx.GetDevice();

	material = std::make_unique<Material>(ctx, *GetShader());
	material->
		AddBinding<Setting>(0).
		AddBinding<WeatherSetting::Atmosphere>(1).
		AddBinding<WeatherSetting::Lighting>(2).
		AddBinding(3, *outputImage).
		AddBinding(4, *opaqueDepthTex, opaqueSampler->GetSampler()).
		AddBinding(5, *opaqueTex, opaqueSampler->GetSampler()).
		AddBinding(6, *shadowMap, shadowSampler->GetSampler()).
		Build(descPool);

	material->UpdateBindingData(0, setting);
}
