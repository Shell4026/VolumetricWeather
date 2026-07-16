#include "render/VulkanBuffer.h"

#include "core/Logger.h"
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
void VulkanBuffer::SetData(const void* data, std::size_t size, std::size_t offset)
{
	if (this->size < size)
	{
		SH_ERROR_FORMAT("Data size({}) is bigger than buffer size({})", size, this->size);
		return;
	}

	if (mapped != nullptr)
		std::memcpy(reinterpret_cast<uint8_t*>(mapped) + offset, data, size);
	else
	{
		Map();
		std::memcpy(reinterpret_cast<uint8_t*>(mapped) + offset, data, size);
	}
}
auto VulkanBuffer::Create(const VulkanContext& ctx, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, const void* data) -> VulkanBuffer
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

auto VulkanBuffer::GetData() const -> void*
{
	return mapped;
}
