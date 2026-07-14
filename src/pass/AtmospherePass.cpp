#include "pass/AtmospherePass.h"
#include "core/Logger.h"
#include "render/VulkanBuffer.h"
#include "render/VulkanImage.h"
#include "render/Material.h"
AtmospherePass::~AtmospherePass()
{
	Clear();
}

void AtmospherePass::Clear()
{
	if (ctx == nullptr)
		return;
	const VkDevice device = ctx->GetDevice();

	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}
	computeShader.Clear();
	material.reset();

	if (descSetLayouts.size() >= 2)
		vkDestroyDescriptorSetLayout(device, descSetLayouts[1], nullptr);
	descSetLayouts.clear();

	opaqueSampler.Clear();
	outputImage.reset();
	APass::Clear();
}

void AtmospherePass::Record(const VulkanContext& ctx, const FrameContext& frame)
{
	const VkCommandBuffer cmd = GetCommandBuffer();
	const uint32_t width = outputImage->GetInfo().extent.width;
	const uint32_t height = outputImage->GetInfo().extent.height;

	vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	std::array<VkDescriptorSet, 2> descSets = { frame.cameraSet, material->GetVkDescriptorSet() };
	vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, computeShader.GetPipelineLayout(), 0, descSets.size(), descSets.data(), 0, nullptr);
	vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 16.f)), static_cast<uint32_t>(std::ceil(height / 16.f)), 1);
}

void AtmospherePass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	APass::SetUsages(ctx, frame);
	AddUsage(outputImage->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
	AddUsage(
		opaqueDepthTex->GetImage(), 
		VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT | VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT, 
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

void AtmospherePass::SetAtmosphere(const Atmosphere& atmosphere)
{
	this->atmosphere = atmosphere;
	material->UpdateBindingData(0, this->atmosphere);
}

void AtmospherePass::PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout)
{
	this->cameraSetLayout = cameraSetLayout;
	const VkDevice device = ctx.GetDevice();

	glm::vec3 sunDir = glm::normalize(glm::vec3{ -1.f, 0.f, 0.f });
	atmosphere.sun = glm::vec4{ sunDir, 20.f };

	VkImageCreateInfo imgCi = VulkanImage::GetCreateInfo();
	imgCi.format = VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT;
	imgCi.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT;
	imgCi.extent = { 1024, 768, 1 };
	outputImage = std::make_unique<VulkanImage>(ctx, imgCi, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkSamplerCreateInfo samplerCI = VulkanSampler::GetCreateInfo();
	samplerCI.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerCI.addressModeV = samplerCI.addressModeU;
	samplerCI.addressModeW = samplerCI.addressModeU;
	samplerCI.borderColor = VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	opaqueSampler.Create(ctx, samplerCI);

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

	computeShader.AddSet(0, cameraSetLayout);
	computeShader.AddSet(1, std::move(set1Bindings));
	computeShader.Build(device, "shaders/atmosphere.comp.spv");
}

void AtmospherePass::SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	const VkDevice device = ctx.GetDevice();

	material = std::make_unique<Material>(ctx, computeShader);
	material->AddBinding<Atmosphere>(0);
	material->AddBinding(1, *outputImage);
	material->AddBinding(2, *opaqueDepthTex, opaqueSampler.GetSampler());
	material->AddBinding(3, *opaqueTex, opaqueSampler.GetSampler());
	material->AddBinding(4, *shadowMap, shadowSampler->GetSampler());
	material->Build(descPool);

	material->UpdateBindingData(0, atmosphere);
}

void AtmospherePass::BuildPipeline(const VulkanContext& ctx)
{
	VkComputePipelineCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.layout = computeShader.GetPipelineLayout();
	ci.stage = computeShader.GetPipelineShaderStageCreateInfos().front();
	VK_RESULT_CHECK(vkCreateComputePipelines(ctx.GetDevice(), nullptr, 1, &ci, nullptr, &pipeline));
}
