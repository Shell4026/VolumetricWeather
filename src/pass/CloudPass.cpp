#include "pass/CloudPass.h"

#include "core/Logger.h"
#include "core/Noise.h"
#include "core/Util.h"

#include "render/Shader.h"
#include "render/Material.h"

#include <filesystem>
void CloudPass::Clear()
{
	material.reset();
	shader.reset();
	output.reset();
	perlin.reset();
	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(ctx->GetDevice(), pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}
}
void CloudPass::Record(const VulkanContext& ctx, const FrameContext& frame)
{
	if (bSettingDirty)
	{
		material->UpdateBindingData(0, setting);
		bSettingDirty = false;
	}

	const VkCommandBuffer cmd = GetCommandBuffer();
	vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	const uint32_t width = output->GetInfo().extent.width;
	const uint32_t height = output->GetInfo().extent.height;
	const std::array<VkDescriptorSet, 2> descSets = { frame.cameraSet, material->GetVkDescriptorSet() };
	vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, shader->GetPipelineLayout(), 0, descSets.size(), descSets.data(), 0, nullptr);
	vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 16.f)), static_cast<uint32_t>(std::ceil(height / 16.f)), 1.f);
}

void CloudPass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	APass::SetUsages(ctx, frame);
	AddUsage(output->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
	AddUsage(perlin->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(worley->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(sceneDepth->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void CloudPass::SetSetting(const Setting& setting)
{
	this->setting = setting;
	bSettingDirty = true;
}

void CloudPass::PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout)
{
	VkImageCreateInfo ci = VulkanImage::GetCreateInfo();
	ci.extent = { ctx.GetSwapChainExtent().width, ctx.GetSwapChainExtent().height, 1};
	ci.format = VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT;
	ci.imageType = VkImageType::VK_IMAGE_TYPE_2D;
	ci.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT;
	output = std::make_unique<VulkanImage>(ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	LoadNoises();

	std::vector<VkDescriptorSetLayoutBinding> set1Bindings;
	set1Bindings.reserve(3);
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
	shader = std::make_unique<Shader>();
	shader->
		AddSet(0, cameraSetLayout).
		AddSet(1, std::move(set1Bindings)).
		Build(ctx.GetDevice(), "shaders/cloud.comp.spv");

	sampler = &samplerManager->GetLinearRepeat();
	depthSampler = &samplerManager->GetLinearClampWhite();
}

void CloudPass::SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	material = std::make_unique<Material>(ctx, *shader);
	material->
		AddBinding<Setting>(0).
		AddBinding(1, *output).
		AddBinding(2, *perlin, sampler->GetSampler()).
		AddBinding(3, *worley, sampler->GetSampler()).
		AddBinding(4, *sceneDepth, depthSampler->GetSampler()).
		Build(descPool);

	material->UpdateBindingData(0, setting);
}

void CloudPass::BuildPipeline(const VulkanContext& ctx)
{
	VkComputePipelineCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.layout = shader->GetPipelineLayout();
	ci.stage = shader->GetPipelineShaderStageCreateInfos().front();
	VK_RESULT_CHECK(vkCreateComputePipelines(ctx.GetDevice(), nullptr, 1, &ci, nullptr, &pipeline));
}

void CloudPass::LoadNoises()
{
	VkImageCreateInfo ci = VulkanImage::GetCreateInfo();
	ci.extent = { 128, 128, 128 };
	ci.format = VkFormat::VK_FORMAT_R8_UNORM;
	ci.imageType = VkImageType::VK_IMAGE_TYPE_3D;
	ci.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	perlin = std::make_unique<VulkanImage>(*ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	worley = std::make_unique<VulkanImage>(*ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	Noise::Texel perlinNoise;
	const std::filesystem::path perlinPath = "textures/perlinWorley.bin";
	if (std::filesystem::exists(perlinPath))
	{
		perlinNoise = util::LoadBinary(perlinPath);
		if (perlinNoise.size() != 128 * 128 * 128)
		{
			SH_ERROR_FORMAT("Data size is wrong!: {} / {}", perlinNoise.size(), 128 * 128 * 128);
			throw std::runtime_error{ "Data size is wrong!" };
		}
	}
	else
	{
		perlinNoise = Noise::GeneratePerlinWorleyNoiseTexture(128, 128, 128, 8);
		util::SaveBinary(perlinNoise, perlinPath);
	}
	perlin->SetData(perlinNoise.data());

	Noise::Texel worleyNoise;
	const std::filesystem::path worleyPath = "textures/worley.bin";
	if (std::filesystem::exists(worleyPath))
	{
		worleyNoise = util::LoadBinary(worleyPath);
		if (worleyNoise.size() != 128 * 128 * 128)
		{
			SH_ERROR_FORMAT("Data size is wrong!: {} / {}", worleyNoise.size(), 128 * 128 * 128);
			throw std::runtime_error{ "Data size is wrong!" };
		}
	}
	else
	{
		worleyNoise = Noise::GenerateWorleyNoiseTexture(128, 128, 128);
		util::SaveBinary(worleyNoise, worleyPath);
	}
	worley->SetData(worleyNoise.data());
}
