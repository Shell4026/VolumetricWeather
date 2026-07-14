#pragma once
#include "Shader.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"

#include <memory>
#include <vector>
#include <variant>
#include <map>
class Material
{
public:
	struct UsingTexture
	{
		const VulkanImage* imagePtr = nullptr;
		VkImageLayout usage = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
	};
public:
	Material(const VulkanContext& ctx, const Shader& shader) :
		ctx(ctx), shader(shader)
	{}
	virtual ~Material();

	void Build(VkDescriptorPool descPool);
	void Clear();

	template<typename T>
	void AddBinding(uint32_t binding);
	void AddBinding(uint32_t binding, const VulkanImage& image, VkSampler sampler = nullptr);
	template<typename T>
	void UpdateBindingData(uint32_t binding, const T& data);
	void UpdateBindingData(uint32_t binding, const VulkanImage& image, VkSampler sampler);

	auto GetVkDescriptorSet() const -> VkDescriptorSet { return descSet; }
	auto GetUsingTextures() const -> const std::map<VkImageView, UsingTexture>& { return usingTextures; }
protected:
	auto CreateUniformBuffer(std::size_t size) -> VulkanBuffer;
public:
	const VulkanContext& ctx;
	const Shader& shader;
private:
	VkDescriptorPool descPool = VK_NULL_HANDLE;
	VkDescriptorSet descSet = VK_NULL_HANDLE; // 메테리얼의 set번호는 1 고정

	struct BindingInfo
	{
		uint32_t binding = 0;
		VkDescriptorType type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		std::variant<VkDescriptorBufferInfo, VkDescriptorImageInfo> info;
	};
	std::vector<BindingInfo> bindingInfos;
	std::map<VkImageView, UsingTexture> usingTextures;
	std::unique_ptr<VulkanBuffer> buffer;

	std::size_t nextBufferOffset = 0;
};

template<typename T>
inline void Material::AddBinding(uint32_t binding)
{
	if (bindingInfos.size() <= binding)
		bindingInfos.resize(binding + 1);

	BindingInfo& bindingInfo = bindingInfos[binding];
	bindingInfo.binding = binding;
	bindingInfo.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = VK_NULL_HANDLE;
	bufferInfo.offset = nextBufferOffset;
	bufferInfo.range = sizeof(T);

	bindingInfo.info = bufferInfo;

	nextBufferOffset += sizeof(T);
}

template<typename T>
inline void Material::UpdateBindingData(uint32_t binding, const T& data)
{
	if (bindingInfos.size() <= binding || buffer == nullptr)
		return;
	BindingInfo& bindingInfo = bindingInfos[binding];
	VkDescriptorBufferInfo& bufferInfo = std::get<VkDescriptorBufferInfo>(bindingInfo.info);

	if (bufferInfo.range != sizeof(data))
	{
		SH_ERROR_FORMAT("Data size({}) is not equal buffer size({})", sizeof(data), bufferInfo.range);
		return;
	}
	buffer->SetData(&data, sizeof(data), bufferInfo.offset);
}
