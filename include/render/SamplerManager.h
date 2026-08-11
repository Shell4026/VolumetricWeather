#pragma once
#include "VulkanImage.h"

#include "core/Util.h"

#include <unordered_map>
#include <memory>
class VulkanContext;
class SamplerManager
{
public:
	SamplerManager(const VulkanContext& ctx);
	~SamplerManager();

	void Clear();

    auto GetSamplerOrCreate(const VkSamplerCreateInfo& ci) -> VulkanSampler*;
    auto GetLinearRepeat() const -> VulkanSampler& { return *linearRepeat; }
    auto GetLinearClamp() const -> VulkanSampler& { return *linearClamp; }
    auto GetLinearClampBlack() const -> VulkanSampler& { return *linearClampBlack; }
    auto GetLinearClampWhite() const -> VulkanSampler& { return *linearClampWhite; }
    auto GetPointRepeat() const -> VulkanSampler& { return *pointRepeat; }
    auto GetPointClampWhite() const -> VulkanSampler& { return *pointClampWhite; }
private:
    struct SamplerInfo
    {
        VkFilter                magFilter;
        VkFilter                minFilter;
        VkSamplerMipmapMode     mipmapMode;
        VkSamplerAddressMode    addressModeU;
        VkSamplerAddressMode    addressModeV;
        VkSamplerAddressMode    addressModeW;
        float                   mipLodBias;
        VkBool32                anisotropyEnable;
        float                   maxAnisotropy;
        VkBool32                compareEnable;
        VkCompareOp             compareOp;
        float                   minLod;
        float                   maxLod;
        VkBorderColor           borderColor;
        VkBool32                unnormalizedCoordinates;

        auto operator==(const SamplerInfo& other) const noexcept -> bool
        {
            return
                magFilter == other.magFilter &&
                minFilter == other.minFilter &&
                mipmapMode == other.mipmapMode &&
                addressModeU == other.addressModeU &&
                addressModeV == other.addressModeV &&
                addressModeW == other.addressModeW &&
                mipLodBias == other.mipLodBias &&
                anisotropyEnable == other.anisotropyEnable &&
                maxAnisotropy == other.maxAnisotropy &&
                compareEnable == other.compareEnable &&
                compareOp == other.compareOp &&
                minLod == other.minLod &&
                maxLod == other.maxLod &&
                borderColor == other.borderColor &&
                unnormalizedCoordinates == other.unnormalizedCoordinates;
        }
    };
	struct CIHasher
	{
		auto operator()(const SamplerInfo& ci) const noexcept -> std::size_t
		{
            std::hash<int> intHasher{};

			std::size_t hash = intHasher(ci.minFilter);
            hash = util::CombineHash(hash, intHasher(ci.magFilter));
            hash = util::CombineHash(hash, intHasher(ci.mipmapMode));
            hash = util::CombineHash(hash, intHasher(ci.addressModeU));
            hash = util::CombineHash(hash, intHasher(ci.addressModeV));
            hash = util::CombineHash(hash, intHasher(ci.addressModeW));
            hash = util::CombineHash(hash, intHasher(std::bit_cast<int>(ci.mipLodBias)));
            hash = util::CombineHash(hash, intHasher(std::bit_cast<int>(ci.maxAnisotropy)));
            hash = util::CombineHash(hash, intHasher(ci.compareEnable));
            hash = util::CombineHash(hash, intHasher(ci.compareOp));
            hash = util::CombineHash(hash, intHasher(std::bit_cast<int>(ci.minLod)));
            hash = util::CombineHash(hash, intHasher(std::bit_cast<int>(ci.maxLod)));
            hash = util::CombineHash(hash, intHasher(ci.borderColor));
            hash = util::CombineHash(hash, intHasher(ci.unnormalizedCoordinates));
            return hash;
		}
	};
    const VulkanContext& ctx;

	std::unordered_map<SamplerInfo, std::unique_ptr<VulkanSampler>, CIHasher> samplers;

    VulkanSampler* linearRepeat = nullptr;
    VulkanSampler* linearClamp = nullptr;
    VulkanSampler* linearClampBlack = nullptr;
    VulkanSampler* linearClampWhite = nullptr;
    VulkanSampler* pointRepeat = nullptr;
    VulkanSampler* pointClampWhite = nullptr;
};