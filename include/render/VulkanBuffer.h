#pragma once

#include "VulkanContext.h"
class VulkanBuffer
{
public:
	VulkanBuffer(VulkanBuffer& other) = delete;
	VulkanBuffer(VulkanBuffer&& other) noexcept :
		device(other.device),
		buffer(other.buffer),
		mem(other.mem),
		descriptorInfo(other.descriptorInfo),
		size(other.size),
		alignment(other.alignment),
		mapped(other.mapped)
	{
		other.buffer = VK_NULL_HANDLE;
		other.mem = VK_NULL_HANDLE;
		other.mapped = nullptr;
	}
	~VulkanBuffer();

	void Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void UnMap();
	void Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void Bind(VkDeviceSize offset = 0);
	void SetData(const void* data, std::size_t size, std::size_t offset = 0);

	static auto Create(const VulkanContext& ctx, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, const void* data = nullptr) -> VulkanBuffer;

	auto GetData() const -> void*;
	auto GetBuffer() const -> VkBuffer { return buffer; }
	auto GetDescriptorBufferInfo() const -> const VkDescriptorBufferInfo& { return descriptorInfo; }
	auto GetSize() const -> VkDeviceSize { return size; }
protected:
	VulkanBuffer() = default;
private:
	VkDevice device = VK_NULL_HANDLE;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory mem = VK_NULL_HANDLE;
	VkDescriptorBufferInfo descriptorInfo;
	VkDeviceSize size = 0;
	VkDeviceSize alignment = 0;
	void* mapped = nullptr;
};