#pragma once
#include "VulkanContext.h"

#include "glm/vec4.hpp"

#include <cstdint>
#include <map>
#include <vector>
class VulkanSampler
{
public:
	VulkanSampler() = default;
	VulkanSampler(const VulkanContext& ctx, const VkSamplerCreateInfo& ci) { Create(ctx, ci); }
	VulkanSampler(const VulkanSampler& other) = delete;
	VulkanSampler(VulkanSampler&& other) noexcept :
		device(other.device),
		sampler(other.sampler),
		info(other.info)
	{
		other.sampler = VK_NULL_HANDLE;
	}
	~VulkanSampler();

	void Create(const VulkanContext& ctx, const VkSamplerCreateInfo& ci);
	void Clear();

	auto GetSampler() const -> VkSampler { return sampler; }
	auto GetInfo() const -> const VkSamplerCreateInfo& { return info; }

	static auto GetCreateInfo() -> VkSamplerCreateInfo;
private:
	VkDevice device = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VkSamplerCreateInfo info{};
};

class VulkanImage
{
public:
	VulkanImage() = default;
	VulkanImage(const VulkanContext& ctx, const VkImageCreateInfo& ci, VkImageAspectFlags aspect, VkMemoryPropertyFlags memProp);
	VulkanImage(const VulkanImage& other) = delete;
	VulkanImage(VulkanImage&& other) noexcept;
	~VulkanImage();

	void Create(const VulkanContext& ctx, const VkImageCreateInfo& ci, VkImageAspectFlags aspect, VkMemoryPropertyFlags memProp);
	void SetData(const uint8_t* dataPtr, std::size_t size, uint32_t mip = 0, uint32_t arrayIdx = 0);

	auto GetImage() const -> VkImage { return img; }
	auto GetView() const -> VkImageView { return view; }
	auto GetViews() const -> const std::vector<VkImageView>& { return views; }
	auto GetMemory() const -> VkDeviceMemory { return mem; }
	auto GetInfo() const -> const VkImageCreateInfo& { return info; }
	auto IsArray() const -> bool { return !views.empty(); }

	static auto GetCreateInfo() -> VkImageCreateInfo;
	static auto GetVulkanImageUsingHandle(VkImage handle) -> VulkanImage*;
private:
	const VulkanContext* ctx = nullptr;
	VkImage img = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	std::vector<VkImageView> views; // array전용
	VkDeviceMemory mem = VK_NULL_HANDLE;
	VkImageCreateInfo info{};

	std::size_t bufferSize = 0;

	static std::map<VkImage, VulkanImage*> handleMap;
};