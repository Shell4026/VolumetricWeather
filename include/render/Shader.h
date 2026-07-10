#pragma once
#include "VulkanContext.h"

#include <filesystem>
#include <vector>
#include <variant>
class Shader
{
public:
	using SetInfo = std::variant<VkDescriptorSetLayout, std::vector<VkDescriptorSetLayoutBinding>>;
public:
	Shader() = default;
	Shader(Shader&& other) noexcept;
	~Shader() { Clear(); }
	
	void Clear();
	
	void AddSet(uint32_t set, VkDescriptorSetLayout setLayout);
	void AddSet(uint32_t set, std::vector<VkDescriptorSetLayoutBinding> bindingInfo);

	void Build(VkDevice device, const std::filesystem::path& vertShaderPath, const std::filesystem::path& fragShaderPath, VkPushConstantRange* pushConstant = nullptr);

	auto GetSetInfos() const -> const std::vector<SetInfo>& { return setInfos; }
	auto GetDescriptorSetLayouts() const -> const std::vector<VkDescriptorSetLayout>& { return descSetLayouts; }
	auto GetPipelineLayout() const -> VkPipelineLayout { return pipelineLayout; }
	auto GetPipelineShaderStageCreateInfos() const -> const std::vector<VkPipelineShaderStageCreateInfo>& { return pipelineShaderStageCreateInfos; }
private:
	void LoadShaderModule(const std::filesystem::path& vertShader, const std::filesystem::path& fragShader);
private:
	VkDevice device = VK_NULL_HANDLE;

	std::vector<SetInfo> setInfos;
	std::vector<VkDescriptorSetLayout> descSetLayouts;
	std::vector<bool> descSetLayoutsOwnerships;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	VkShaderModule vertShader = VK_NULL_HANDLE;
	VkShaderModule fragShader = VK_NULL_HANDLE;
	std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStageCreateInfos;
};