#include "render/Material.h"
#include "core/Logger.h"

Material::~Material()
{
	Clear();
}

void Material::Build(VkDescriptorPool descPool)
{
	this->descPool = descPool;
	// 디스크립터셋 생성
	VkDescriptorSetAllocateInfo descSetAllocInfo{};
	descSetAllocInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descSetAllocInfo.descriptorPool = descPool;
	descSetAllocInfo.pSetLayouts = &shader.GetDescriptorSetLayouts()[1]; // 0은 카메라 같은 전역셋
	descSetAllocInfo.descriptorSetCount = 1;
	VK_RESULT_CHECK(vkAllocateDescriptorSets(ctx.GetDevice(), &descSetAllocInfo, &descSet));

	// 버퍼 생성
	const std::size_t size = nextBufferOffset;
	buffer = std::make_unique<VulkanBuffer>(CreateUniformBuffer(size));

	// BufferInfo토대로 VkWriteDescriptorSet생성 후 디스크립터셋하고 데이터 연결
	std::vector<VkWriteDescriptorSet> writes(bindingInfos.size());
	for (std::size_t i = 0; i < bindingInfos.size(); ++i)
	{
		BindingInfo& bindingInfo = bindingInfos[i];

		VkWriteDescriptorSet& write = writes[i];
		write.sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = bindingInfo.type;
		write.descriptorCount = 1;
		write.dstSet = descSet;
		write.dstBinding = bindingInfo.binding;

		if (std::holds_alternative<VkDescriptorBufferInfo>(bindingInfo.info))
		{
			VkDescriptorBufferInfo& bufferInfo = std::get<VkDescriptorBufferInfo>(bindingInfo.info);
			bufferInfo.buffer = buffer->GetBuffer();
			write.pBufferInfo = &bufferInfo;
		}
		else
		{
			VkDescriptorImageInfo& imageInfo = std::get<VkDescriptorImageInfo>(bindingInfo.info);
			write.pImageInfo = &imageInfo;
		}
	}
	if (!writes.empty())
		vkUpdateDescriptorSets(ctx.GetDevice(), writes.size(), writes.data(), 0, nullptr);
}

void Material::Clear()
{
	bindingInfos.clear();
	buffer.reset();
	if (descSet != VK_NULL_HANDLE)
	{
		vkFreeDescriptorSets(ctx.GetDevice(), descPool, 1, &descSet);
		descSet = VK_NULL_HANDLE;
	}
}

void Material::AddBinding(uint32_t binding, const VulkanImage& image, VkSampler sampler)
{
	if (bindingInfos.size() <= binding)
		bindingInfos.resize(binding + 1);

	BindingInfo& bindingInfo = bindingInfos[binding];
	bindingInfo.binding = binding;
	bindingInfo.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = image.GetView();
	imageInfo.sampler = sampler;

	bindingInfo.info = imageInfo;

	usingTextures.insert_or_assign(imageInfo.imageView, &image);
}

void Material::UpdateBindingData(uint32_t binding, const VulkanImage& image, VkSampler sampler)
{
	if (bindingInfos.size() <= binding)
		return;

	BindingInfo& bindingInfo = bindingInfos[binding];
	VkDescriptorImageInfo& imageInfo = std::get<VkDescriptorImageInfo>(bindingInfo.info);

	usingTextures.erase(imageInfo.imageView);

	imageInfo.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = image.GetView();
	imageInfo.sampler = sampler;

	usingTextures.insert_or_assign(imageInfo.imageView, &image);

	VkWriteDescriptorSet write{};
	write.sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorType = bindingInfo.type;
	write.descriptorCount = 1;
	write.dstSet = descSet;
	write.dstBinding = bindingInfo.binding;
	write.pImageInfo = &imageInfo;
	vkUpdateDescriptorSets(ctx.GetDevice(), 1, &write, 0, nullptr);
}

auto Material::CreateUniformBuffer(std::size_t size) -> VulkanBuffer
{
	return VulkanBuffer::Create(
		ctx, 
		VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 
		size);
}