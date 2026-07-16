#pragma once
#include "VulkanContext.h"

#include "glm/vec4.hpp"

#include <cstdint>
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
	VulkanImage(VulkanImage&& other) noexcept :
		ctx(other.ctx),
		img(other.img),
		view(other.view),
		mem(other.mem),
		info(other.info),
		bufferSize(other.bufferSize)
	{
		other.img = VK_NULL_HANDLE;
		other.view = VK_NULL_HANDLE;
		other.mem = VK_NULL_HANDLE;
	}
	~VulkanImage();

	void Create(const VulkanContext& ctx, const VkImageCreateInfo& ci, VkImageAspectFlags aspect, VkMemoryPropertyFlags memProp);
	void SetData(const uint8_t* dataPtr);

	auto GetImage() const -> VkImage { return img; }
	auto GetView() const -> VkImageView { return view; }
	auto GetMemory() const -> VkDeviceMemory { return mem; }
	auto GetInfo() const -> const VkImageCreateInfo& { return info; }

	static auto GetCreateInfo() -> VkImageCreateInfo;
private:
	const VulkanContext* ctx = nullptr;
	VkImage img = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkDeviceMemory mem = VK_NULL_HANDLE;
	VkImageCreateInfo info{};

	std::size_t bufferSize = 0;
};