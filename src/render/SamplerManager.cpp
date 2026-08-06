#include "render/SamplerManager.h"

SamplerManager::SamplerManager(const VulkanContext& ctx) :
    ctx(ctx)
{
    {
        VkSamplerCreateInfo ci = VulkanSampler::GetCreateInfo();
        ci.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT;
        ci.addressModeV = ci.addressModeU;
        ci.addressModeW = ci.addressModeU;
        linearRepeat = GetSamplerOrCreate(ci);
        ci.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        ci.addressModeV = ci.addressModeU;
        ci.addressModeW = ci.addressModeU;
        linearClamp = GetSamplerOrCreate(ci);
        ci.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        ci.addressModeV = ci.addressModeU;
        ci.addressModeW = ci.addressModeU;
        ci.borderColor = VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        linearClampBlack = GetSamplerOrCreate(ci);
        ci.borderColor = VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        linearClampWhite = GetSamplerOrCreate(ci);
    }
    {
        VkSamplerCreateInfo ci = VulkanSampler::GetCreateInfo();
        ci.minFilter = VkFilter::VK_FILTER_NEAREST;
        ci.magFilter = VkFilter::VK_FILTER_NEAREST;
        ci.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT;
        ci.addressModeV = ci.addressModeU;
        ci.addressModeW = ci.addressModeU;
        pointRepeat = GetSamplerOrCreate(ci);
        ci.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        ci.addressModeV = ci.addressModeU;
        ci.addressModeW = ci.addressModeU;
        ci.borderColor = VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        pointClampWhite = GetSamplerOrCreate(ci);
    }
}

SamplerManager::~SamplerManager()
{
	Clear();
}

void SamplerManager::Clear()
{
	samplers.clear();
}

auto SamplerManager::GetSamplerOrCreate(const VkSamplerCreateInfo& ci) -> VulkanSampler*
{
	SamplerInfo info{};
    info.magFilter = ci.magFilter;
    info.minFilter = ci.minFilter;
    info.mipmapMode = ci.mipmapMode;
    info.addressModeU = ci.addressModeU;
    info.addressModeV = ci.addressModeV;
    info.addressModeW = ci.addressModeW;
    info.mipLodBias = ci.mipLodBias;
    info.anisotropyEnable = ci.anisotropyEnable;
    info.maxAnisotropy = ci.maxAnisotropy;
    info.compareEnable = ci.compareEnable;
    info.compareOp = ci.compareOp;
    info.minLod = ci.minLod;
    info.maxLod = ci.maxLod;
    info.borderColor = ci.borderColor;
    info.unnormalizedCoordinates = ci.unnormalizedCoordinates;

    auto it = samplers.find(info);
    if (it != samplers.end())
        return it->second.get();

    return samplers.insert({ info, std::make_unique<VulkanSampler>(ctx, ci) }).first->second.get();
}
