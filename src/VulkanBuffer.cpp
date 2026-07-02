#include "VulkanBuffer.h"

#include "Logger.h"
VulkanBuffer::~VulkanBuffer()
{
	if (buffer != nullptr)
	{
		vkDestroyBuffer(device, buffer, nullptr);
		buffer = VK_NULL_HANDLE;
	}
	if (mem != nullptr)
	{
		vkFreeMemory(device, mem, nullptr);
		mem = VK_NULL_HANDLE;
	}
}
void VulkanBuffer::Map(VkDeviceSize size, VkDeviceSize offset)
{
	VK_RESULT_CHECK(vkMapMemory(device, mem, offset, size, 0, &mapped));
}
void VulkanBuffer::UnMap()
{
	if (mapped != nullptr)
	{
		vkUnmapMemory(device, mem);
		mapped = nullptr;
	}
}
void VulkanBuffer::Flush(VkDeviceSize size, VkDeviceSize offset)
{
	VkMappedMemoryRange mappedRange{
	.sType = VkStructureType::VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
	.memory = mem,
	.offset = offset,
	.size = size
	};
	VK_RESULT_CHECK(vkFlushMappedMemoryRanges(device, 1, &mappedRange));
}
void VulkanBuffer::Bind(VkDeviceSize offset)
{
	VK_RESULT_CHECK(vkBindBufferMemory(device, buffer, mem, offset));
}
void VulkanBuffer::SetData(void* data)
{
	if (mapped != nullptr)
		std::memcpy(mapped, data, size);
	else
	{
		Map();
		std::memcpy(mapped, data, size);
	}
}
auto VulkanBuffer::Create(const VulkanContext& ctx, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void* data) -> VulkanBuffer
{
	VulkanBuffer buffer{};
	buffer.device = ctx.GetDevice();

	VkBufferCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	ci.usage = usage;
	ci.size = size;
	VK_RESULT_CHECK(vkCreateBuffer(ctx.GetDevice(), &ci, nullptr, &buffer.buffer));

	VkMemoryRequirements memReqs{};
	vkGetBufferMemoryRequirements(ctx.GetDevice(), buffer.buffer, &memReqs);

	std::optional<uint32_t> memoryTypeIdx = ctx.GetMemoryTypeIndex(memReqs.memoryTypeBits, memoryPropertyFlags);
	if (!memoryTypeIdx.has_value())
	{
		SH_ERROR("Could not find a matching memory type!");
		throw std::runtime_error{ "Could not find a matching memory type!" };
	}
	VkMemoryAllocateInfo memAllocInfo{};
	memAllocInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = memoryTypeIdx.value();
	VK_RESULT_CHECK(vkAllocateMemory(ctx.GetDevice(), &memAllocInfo, nullptr, &buffer.mem));

	buffer.size = size;
	buffer.alignment = memReqs.alignment;

	if (data != nullptr)
	{
		buffer.Map();
		std::memcpy(buffer.mapped, data, size);
		if ((memoryPropertyFlags & VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
			buffer.Flush();
		buffer.UnMap();
	}
	buffer.descriptorInfo.buffer = buffer.buffer;
	buffer.descriptorInfo.range = VK_WHOLE_SIZE;
	buffer.descriptorInfo.offset = 0;
	buffer.Bind();

	return buffer;
}
