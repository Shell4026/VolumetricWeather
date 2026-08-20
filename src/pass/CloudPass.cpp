#include "pass/CloudPass.h"

#include "core/Logger.h"
#include "core/Noise.h"
#include "core/Util.h"

#include "render/Camera.h"
#include "render/Shader.h"
#include "render/Material.h"

#include <filesystem>
void CloudPass::Clear()
{
	material.reset();
	shader.reset();
	output.reset();
	depth.reset();
	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(ctx->GetDevice(), pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}
}

void CloudPass::SetUsages(const FrameContext& frame)
{
	APass::SetUsages(frame);

	AddUsage(output->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
	AddUsage(depth->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
	AddUsage(perlin->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	AddUsage(noiseTex->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(sceneDepth->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(transmittanceLUT->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	AddUsage(cloudMask->GetImage(), VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void CloudPass::BeginRecord(const FrameContext& frame, const std::vector<BarrierInfo>* barrierInfos)
{
	APass::BeginRecord(frame, barrierInfos);

	frameIdx = frameIdx + 1;
	time += timeSpeed * frame.dt;
	setting.frame = frameIdx;
	setting.time = time;
	material->UpdateBindingData(0, setting);
}
void CloudPass::Record(const FrameContext& frame)
{
	const VkCommandBuffer cmd = GetCommandBuffer();
	vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	const uint32_t width = output->GetInfo().extent.width ;
	const uint32_t height = output->GetInfo().extent.height;
	const std::array<VkDescriptorSet, 2> descSets = { frame.cameraSet, material->GetVkDescriptorSet() };
	vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, shader->GetPipelineLayout(), 0, descSets.size(), descSets.data(), 0, nullptr);
	const float groups = 16.f * 2.f;
	vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / groups)), static_cast<uint32_t>(std::ceil(height / groups)), 1.f);
}

void CloudPass::SetSetting(const WeatherSetting::Atmosphere& atmosphereSetting)
{
	material->UpdateBindingData(1, atmosphereSetting);
}

void CloudPass::SetSetting(const WeatherSetting::Lighting & lightingSetting)
{
	material->UpdateBindingData(2, lightingSetting);
}

void CloudPass::SetSetting(const Setting& setting)
{
	this->setting = setting;
	++settingRevision;
	frameIdx = 0;
}

void CloudPass::PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout)
{
	VkImageCreateInfo ci = VulkanImage::GetCreateInfo();
	ci.extent = { ctx.GetSwapChainExtent().width / 2, ctx.GetSwapChainExtent().height / 2, 1};
	ci.format = VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT;
	ci.imageType = VkImageType::VK_IMAGE_TYPE_2D;
	ci.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT;
	output = std::make_unique<VulkanImage>(ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ci.format = VkFormat::VK_FORMAT_R32_SFLOAT;
	depth = std::make_unique<VulkanImage>(ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	LoadNoises();

	CreateCloudShader(cameraSetLayout);

	sampler = &samplerManager->GetLinearRepeat();
	depthSampler = &samplerManager->GetPointClampWhite();
	pointSampler = &samplerManager->GetPointRepeat();
	maskSampler = &samplerManager->GetLinearClampBlack();
}

void CloudPass::SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	material = std::make_unique<Material>(ctx, *shader);
	material->
		AddBinding<Setting>(0).
		AddBinding<WeatherSetting::Atmosphere>(1).
		AddBinding<WeatherSetting::Lighting>(2).
		AddBinding(3, *output).
		AddBinding(4, *depth).
		AddBinding(5, *perlin, sampler->GetSampler()).
		AddBinding(6, *noiseTex, pointSampler->GetSampler()).
		AddBinding(7, *sceneDepth, depthSampler->GetSampler()).
		AddBinding(8, *transmittanceLUT, transmittanceLUTSampler->GetSampler()).
		AddBinding(9, *cloudMask, maskSampler->GetSampler()).
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

void CloudPass::CreateCloudShader(VkDescriptorSetLayout cameraSetLayout)
{
	std::vector<VkDescriptorSetLayoutBinding> set1Bindings;
	set1Bindings.reserve(10);
	AddDescSetLayoutBinding(set1Bindings, 0, VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 1, VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 2, VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 3, VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 4, VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 5, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 6, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 7, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 8, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
	AddDescSetLayoutBinding(set1Bindings, 9, VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);

	shader = std::make_unique<Shader>();
	shader->
		AddSet(0, cameraSetLayout).
		AddSet(1, std::move(set1Bindings)).
		Build(ctx->GetDevice(), "shaders/cloud.comp.spv");
}

void CloudPass::LoadNoises()
{
	uint32_t width = 128;
	uint32_t height = 128;
	uint32_t depth = 128;

	VkImageCreateInfo ci = VulkanImage::GetCreateInfo();
	ci.extent = { width, height, depth };
	ci.format = VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
	ci.imageType = VkImageType::VK_IMAGE_TYPE_3D;
	ci.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	ci.mipLevels = 8;
	perlin = std::make_unique<VulkanImage>(*ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	std::size_t size = width * height * depth;
	Noise::Texel noiseTexel(size * 4, 0);

	Noise::Texel perlinNoise;
	perlinNoise.reserve(size);
	const std::filesystem::path perlinWorleyPath = "textures/perlinWorley.bin";
	if (std::filesystem::exists(perlinWorleyPath))
	{
		perlinNoise = util::LoadBinary(perlinWorleyPath);
		if (perlinNoise.size() != size)
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

	for (uint32_t m = 0; m < ci.mipLevels; ++m)
	{
		noiseTexel.resize(size * 4, 0);
		for (std::size_t i = 0; i < size; ++i)
		{
			noiseTexel[i * 4 + 0] = perlinNoise[i];
			noiseTexel[i * 4 + 1] = worleyNoises[0][i];
			noiseTexel[i * 4 + 2] = worleyNoises[1][i];
			noiseTexel[i * 4 + 3] = worleyNoises[2][i];
		}
		perlin->SetData(noiseTexel.data(), noiseTexel.size(), m);

		perlinNoise = CreateNextNoiseMip(perlinNoise, width, height, depth);
		worleyNoises[0] = CreateNextNoiseMip(worleyNoises[0], width, height, depth);
		worleyNoises[1] = CreateNextNoiseMip(worleyNoises[1], width, height, depth);
		worleyNoises[2] = CreateNextNoiseMip(worleyNoises[2], width, height, depth);
		width >>= 1;
		height >>= 1;
		depth >>= 1;
		size = width * height * depth;
	}
}

auto CloudPass::CreateNextNoiseMip(const Noise::Texel& noise, uint32_t width, uint32_t height, uint32_t depth) -> std::vector<uint8_t>
{
	if (noise.size() != width * height * depth)
		return {};

	const uint32_t w = width >> 1;
	const uint32_t h = height >> 1;
	const uint32_t d = depth >> 1;

	const std::size_t mipSize = w * h * d;
	if (mipSize == 0)
		return {};

	Noise::Texel mip(mipSize, 0);

	auto srcIndexFn = [&](uint32_t x, uint32_t y, uint32_t z)
		{
			return x + width * y + width * height * z;
		};

	auto dstIndexFn = [&](uint32_t x, uint32_t y, uint32_t z)
		{
			return x + w * y + w * h * z;
		};

	for (int z = 0; z < d; ++z)
	{
		for (int y = 0; y < h; ++y)
		{
			for (int x = 0; x < w; ++x)
			{
				const int wh = w * h;
				int texel = 0;
				for (int dz = 0; dz < 2; ++dz)
					for (int dy = 0; dy < 2; ++dy)
						for (int dx = 0; dx < 2; ++dx)
							texel += noise[srcIndexFn(x * 2 + dx, y * 2 + dy, z * 2 + dz)];
				mip[dstIndexFn(x, y, z)] = texel / 8;
			}
		}
	}
	return mip;
}
