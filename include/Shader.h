#pragma once
#include "VulkanContext.h"

#include <filesystem>
#include <vector>
#include <variant>
class Shader
{
public:
	struct SetInfo
	{
		std::variant<VkDescriptorSetLayout, std::vector<VkDescriptorSetLayoutBinding>> createInfo;
	};
public:
	~Shader() { Clear(); }
	
	void Init(VkDevice device, const std::vector<SetInfo>& setInfos);
	void Clear();
	void LoadShaderModule(const std::filesystem::path& vertShader, const std::filesystem::path& fragShader);

	auto GetDescriptorSetLayouts() const -> const std::vector<VkDescriptorSetLayout>& { return descSetLayouts; }
	auto GetPipelineLayout() const -> VkPipelineLayout { return pipelineLayout; }
	auto GetPipelineShaderStageCreateInfos() const -> const std::vector<VkPipelineShaderStageCreateInfo>& { return pipelineShaderStageCreateInfos; }
private:
	VkDevice device = VK_NULL_HANDLE;

	struct DescriptorSetLayoutInfo
	{
		VkDescriptorSetLayout handle = VK_NULL_HANDLE;
		bool bOwn = false;
	};
	std::vector<VkDescriptorSetLayout> descSetLayouts;
	std::vector<bool> descSetLayoutsOwnerships;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	VkShaderModule vertShader = VK_NULL_HANDLE;
	VkShaderModule fragShader = VK_NULL_HANDLE;
	std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageCreateInfos;
};