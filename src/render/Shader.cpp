#include "render/Shader.h"
#include "core/Logger.h"

#include <fstream>

Shader::Shader(Shader&& other) noexcept :
	device(other.device),
	setInfos(std::move(other.setInfos)),
	descSetLayouts(std::move(other.descSetLayouts)),
	descSetLayoutsOwnerships(std::move(other.descSetLayoutsOwnerships)),
	pipelineLayout(other.pipelineLayout),
	vertShader(other.vertShader),
	fragShader(other.fragShader),
	computeShader(other.computeShader),
	pipelineShaderStageCreateInfos(std::move(other.pipelineShaderStageCreateInfos))
{
	other.device = VK_NULL_HANDLE;
	other.pipelineLayout = VK_NULL_HANDLE;
	other.vertShader = VK_NULL_HANDLE;
	other.fragShader = VK_NULL_HANDLE;
	other.computeShader = VK_NULL_HANDLE;
}

void Shader::Clear()
{
	setInfos.clear();

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
	if (computeShader != VK_NULL_HANDLE)
	{
		vkDestroyShaderModule(device, computeShader, nullptr);
		computeShader = VK_NULL_HANDLE;
	}
	pipelineShaderStageCreateInfos.clear();
}

auto Shader::AddSet(uint32_t set, VkDescriptorSetLayout setLayout) -> Shader&
{
	if (setInfos.size() <= set)
		setInfos.resize(set + 1);

	setInfos[set] = setLayout;
	return *this;
}

auto Shader::AddSet(uint32_t set, std::vector<VkDescriptorSetLayoutBinding> bindingInfo) -> Shader&
{
	if (setInfos.size() <= set)
		setInfos.resize(set + 1);
	
	setInfos[set] = std::move(bindingInfo);
	return *this;
}

void Shader::Build(VkDevice device, const std::filesystem::path& computeShaderPath)
{
	this->device = device;
	
	CreatePipelineLayout(nullptr);
	LoadComputeShaderModule(computeShaderPath);
}

void Shader::Build(VkDevice device, const std::filesystem::path& vertShaderPath, const std::filesystem::path& fragShaderPath, VkPushConstantRange* pushConstant)
{
	this->device = device;

	CreatePipelineLayout(pushConstant);
	LoadShaderModule(vertShaderPath, fragShaderPath);
}

void Shader::CreatePipelineLayout(VkPushConstantRange* pushConstant)
{
	descSetLayouts.resize(setInfos.size());
	descSetLayoutsOwnerships.resize(setInfos.size(), false);
	for (std::size_t i = 0; i < setInfos.size(); ++i)
	{
		if (std::holds_alternative<VkDescriptorSetLayout>(setInfos[i]))
			descSetLayouts[i] = std::get<VkDescriptorSetLayout>(setInfos[i]);
		else
		{
			std::vector<VkDescriptorSetLayoutBinding>& bindings = std::get<std::vector<VkDescriptorSetLayoutBinding>>(setInfos[i]);
			if (bindings.empty())
			{
				VkDescriptorSetLayoutCreateInfo layoutCi{};
				layoutCi.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				VK_RESULT_CHECK(vkCreateDescriptorSetLayout(device, &layoutCi, nullptr, &descSetLayouts[i]));
				descSetLayoutsOwnerships[i] = true;
			}
			else
			{
				VkDescriptorSetLayoutCreateInfo layoutCi{};
				layoutCi.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				layoutCi.bindingCount = static_cast<uint32_t>(bindings.size());
				layoutCi.pBindings = bindings.empty() ? nullptr : bindings.data();
				VK_RESULT_CHECK(vkCreateDescriptorSetLayout(device, &layoutCi, nullptr, &descSetLayouts[i]));
				descSetLayoutsOwnerships[i] = true;
			}
		}
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = descSetLayouts.size();
	pipelineLayoutInfo.pSetLayouts = descSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = pushConstant == nullptr ? 0 : 1;
	pipelineLayoutInfo.pPushConstantRanges = pushConstant;
	VK_RESULT_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));
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

void Shader::LoadComputeShaderModule(const std::filesystem::path& computeShaderPath)
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

	std::vector<char> buffer = LoadBinary(computeShaderPath);
	if (buffer.empty())
		return;

	VkShaderModuleCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = buffer.size();
	ci.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

	VK_RESULT_CHECK(vkCreateShaderModule(device, &ci, nullptr, &computeShader));

	VkPipelineShaderStageCreateInfo& info = pipelineShaderStageCreateInfos.emplace_back();
	info.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	info.module = computeShader;
	info.pName = "main";
}
