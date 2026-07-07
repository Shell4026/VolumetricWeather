#include "Shader.h"
#include "core/Logger.h"

#include <fstream>

void Shader::Init(VkDevice device, const std::vector<SetInfo>& setInfos)
{
	this->device = device;
	descSetLayouts.resize(setInfos.size());
	descSetLayoutsOwnerships.resize(setInfos.size(), false);
	for (std::size_t i = 0; i < setInfos.size(); ++i)
	{
		if (std::holds_alternative<VkDescriptorSetLayout>(setInfos[i].createInfo))
		{
			VkDescriptorSetLayout layout = std::get<VkDescriptorSetLayout>((setInfos[i].createInfo));
			if (layout != VK_NULL_HANDLE)
				descSetLayouts[i] = std::get<VkDescriptorSetLayout>((setInfos[i].createInfo));
			else
			{
				VkDescriptorSetLayoutCreateInfo layoutCi{};
				layoutCi.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				VK_RESULT_CHECK(vkCreateDescriptorSetLayout(device, &layoutCi, nullptr, &descSetLayouts[i]));
				descSetLayoutsOwnerships[i] = true;
			}
		}
		else
		{
			const std::vector<VkDescriptorSetLayoutBinding>& bindings = std::get<std::vector<VkDescriptorSetLayoutBinding>>(setInfos[i].createInfo);
			VkDescriptorSetLayoutCreateInfo layoutCi{};
			layoutCi.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutCi.bindingCount = static_cast<uint32_t>(bindings.size());
			layoutCi.pBindings = bindings.empty() ? nullptr : bindings.data();
			VK_RESULT_CHECK(vkCreateDescriptorSetLayout(device, &layoutCi, nullptr, &descSetLayouts[i]));
			descSetLayoutsOwnerships[i] = true;
		}
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = descSetLayouts.size();
	pipelineLayoutInfo.pSetLayouts = descSetLayouts.data();
	VK_RESULT_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));
}

void Shader::Clear()
{
	if (device == VK_NULL_HANDLE)
		return;

	if (pipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		pipelineLayout = VK_NULL_HANDLE;
	}
	for (std::size_t i = 0; i < descSetLayouts.size(); ++i)
	{
		if (!descSetLayoutsOwnerships[i])
			continue;
		vkDestroyDescriptorSetLayout(device, descSetLayouts[i], nullptr);
	}
	descSetLayouts.clear();
	descSetLayoutsOwnerships.clear();
	if (vertShader != VK_NULL_HANDLE)
	{
		vkDestroyShaderModule(device, vertShader, nullptr);
		vertShader = VK_NULL_HANDLE;
	}
	if (fragShader != VK_NULL_HANDLE)
	{
		vkDestroyShaderModule(device, fragShader, nullptr);
		fragShader = VK_NULL_HANDLE;
	}
	pipelineShaderStageCreateInfos.clear();
}

void Shader::LoadShaderModule(const std::filesystem::path& vertShaderPath, const std::filesystem::path& fragShaderPath)
{
	if (device == VK_NULL_HANDLE)
		return;

	auto LoadBinary = [](const std::filesystem::path& path) -> std::vector<char>
	{
		std::ifstream file(path, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			SH_ERROR_FORMAT("Failed to read shader: {}", path.string());
			return {};
		}
		const std::size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();
		return buffer;
	};
	if (!vertShaderPath.empty())
	{
		std::vector<char> buffer = LoadBinary(vertShaderPath);
		if (buffer.empty())
			return;

		VkShaderModuleCreateInfo ci{};
		ci.sType = VkStructureType::VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		ci.codeSize = buffer.size();
		ci.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

		VK_RESULT_CHECK(vkCreateShaderModule(device, &ci, nullptr, &vertShader));

		VkPipelineShaderStageCreateInfo& info = pipelineShaderStageCreateInfos.emplace_back();
		info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		info.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;
		info.module = vertShader;
		info.pName = "main";
	}
	if (!fragShaderPath.empty())
	{
		std::vector<char> buffer = LoadBinary(fragShaderPath);
		if (buffer.empty())
			return;

		VkShaderModuleCreateInfo ci{};
		ci.sType = VkStructureType::VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		ci.codeSize = buffer.size();
		ci.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

		VK_RESULT_CHECK(vkCreateShaderModule(device, &ci, nullptr, &fragShader));

		VkPipelineShaderStageCreateInfo& info = pipelineShaderStageCreateInfos.emplace_back();
		info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		info.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
		info.module = fragShader;
		info.pName = "main";
	}
}
