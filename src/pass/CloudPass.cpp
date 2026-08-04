#include "pass/CloudPass.h"
#include "Camera.h"

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
	tex1.reset();
	tex2.reset();
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
	const uint32_t width = curOutput->GetInfo().extent.width;
	const uint32_t height = curOutput->GetInfo().extent.height;
	const std::array<VkDescriptorSet, 2> descSets = { frame.cameraSet, material->GetVkDescriptorSet() };
	vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, shader->GetPipelineLayout(), 0, descSets.size(), descSets.data(), 0, nullptr);
	vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 16.f)), static_cast<uint32_t>(std::ceil(height / 16.f)), 1.f);

	trp.pos = frame.cameraPtr->GetPos();
	trp.t = time;
	trp.viewProj = frame.cameraPtr->GetMatrixProj() * frame.cameraPtr->GetMatrixView();
	material->UpdateBindingData(9, trp);
	time = (time + 1) % 0xFFFF;
}

void CloudPass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	APass::SetUsages(ctx, frame);

	const VulkanImage* temp = curOutput;
	curOutput = prevOutput;
	prevOutput = temp;
	temp = curCloudDepth;
	curCloudDepth = prevCloudDepth;
	prevCloudDepth = temp;

	material->UpdateBindingData(1, *curOutput, nullptr);
	material->UpdateBindingData(2, *curCloudDepth, nullptr);
	material->UpdateBindingData(3, *prevOutput, sampler->GetSampler());
	material->UpdateBindingData(4, *prevCloudDepth, sampler->GetSampler());

	AddUsage(curOutput->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
	AddUsage(prevOutput->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(curCloudDepth->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
	AddUsage(prevCloudDepth->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(perlin->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(noiseTex->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(sceneDepth->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(transmittanceLUT->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void CloudPass::SetSetting(const Setting& setting)
{
	this->setting = setting;
	bSettingDirty = true;
}

void CloudPass::PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout)
{
	VkImageCreateInfo ci = VulkanImage::GetCreateInfo();
	ci.extent = { ctx.GetSwapChainExtent().width / 4, ctx.GetSwapChainExtent().height / 4, 1};
	ci.format = VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT;
	ci.imageType = VkImageType::VK_IMAGE_TYPE_2D;
	ci.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT;
	tex1 = std::make_unique<VulkanImage>(ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	tex2 = std::make_unique<VulkanImage>(ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ci.format = VkFormat::VK_FORMAT_R16_SFLOAT;
	cloudDepthHistory1 = std::make_unique<VulkanImage>(ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	cloudDepthHistory2 = std::make_unique<VulkanImage>(ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	curOutput = tex1.get();
	prevOutput = tex2.get();
	curCloudDepth = cloudDepthHistory1.get();
	prevCloudDepth = cloudDepthHistory2.get();

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
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
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
		binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		binding.descriptorCount = 1;
	}
	shader = std::make_unique<Shader>();
	shader->
		AddSet(0, cameraSetLayout).
		AddSet(1, std::move(set1Bindings)).
		Build(ctx.GetDevice(), "shaders/cloud.comp.spv");

	sampler = &samplerManager->GetLinearRepeat();
	depthSampler = &samplerManager->GetLinearClampWhite();
	pointSampler = &samplerManager->GetPointRepeat();
}

void CloudPass::SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	material = std::make_unique<Material>(ctx, *shader);
	material->
		AddBinding<Setting>(0).
		AddBinding(1, *curOutput).
		AddBinding(2, *curCloudDepth).
		AddBinding(3, *prevOutput, sampler->GetSampler()).
		AddBinding(4, *prevCloudDepth, sampler->GetSampler()).
		AddBinding(5, *perlin, sampler->GetSampler()).
		AddBinding(6, *noiseTex, pointSampler->GetSampler()).
		AddBinding(7, *sceneDepth, depthSampler->GetSampler()).
		AddBinding(8, *transmittanceLUT, transmittanceLUTSampler->GetSampler()).
		AddBinding<TRP>(9).
		Build(descPool);

	material->UpdateBindingData(0, setting);
	material->UpdateBindingData(9, trp);
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
	ci.format = VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
	ci.imageType = VkImageType::VK_IMAGE_TYPE_3D;
	ci.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	perlin = std::make_unique<VulkanImage>(*ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	const std::size_t size = 128 * 128 * 128;
	Noise::Texel noiseTexel(size * 4, 0);

	Noise::Texel perlinNoise;
	perlinNoise.reserve(size);
	const std::filesystem::path perlinWorleyPath = "textures/perlinWorley.bin";
	if (std::filesystem::exists(perlinWorleyPath))
	{
		perlinNoise = util::LoadBinary(perlinWorleyPath);
		if (perlinNoise.size() != 128 * 128 * 128)
		{
			SH_ERROR_FORMAT("Data size is wrong!: {} / {}", perlinNoise.size(), size);
			throw std::runtime_error{ "Data size is wrong!" };
		}
	}
	else
	{
		perlinNoise = Noise::GeneratePerlinWorleyNoiseTexture(128, 128, 128, 8);
		util::SaveBinary(perlinNoise, perlinWorleyPath);
	}

	std::array<Noise::Texel, 3> worleyNoises;
	float f0 = 1;
	float f1 = 2;
	float f2 = 4;
	for (int i = 0; i < 3; ++i)
	{
		worleyNoises[i].reserve(size);

		const std::filesystem::path worleyPath = std::format("textures/worley{}.bin", i);
		if (std::filesystem::exists(worleyPath))
		{
			worleyNoises[i] = util::LoadBinary(worleyPath);
			if (worleyNoises[i].size() != size)
			{
				SH_ERROR_FORMAT("Data size is wrong!: {} / {}", worleyNoises[i].size(), size);
				throw std::runtime_error{ "Data size is wrong!" };
			}
		}
		else
		{
			worleyNoises[i] = Noise::GenerateWorleyNoiseTexture(128, 128, 128, f0, f1, f2);
			util::SaveBinary(worleyNoises[i], worleyPath);
		}
		f0 *= 2;
		f1 *= 2;
		f2 *= 2;
	}

	for (std::size_t i = 0; i < size; ++i)
	{
		noiseTexel[i * 4 + 0] = perlinNoise[i];
		noiseTexel[i * 4 + 1] = worleyNoises[0][i];
		noiseTexel[i * 4 + 2] = worleyNoises[1][i];
		noiseTexel[i * 4 + 3] = worleyNoises[2][i];
	}
	perlin->SetData(noiseTexel.data());
}
