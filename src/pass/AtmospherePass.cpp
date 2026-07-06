#include "pass/AtmospherePass.h"
#include "Logger.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"

void AtmospherePass::Clear(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	APass::Clear(ctx, descPool);
	const VkDevice device = ctx.GetDevice();

	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}
	if (pipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		pipelineLayout = VK_NULL_HANDLE;
	}
	if (computeShader != VK_NULL_HANDLE)
	{
		vkDestroyShaderModule(device, computeShader, nullptr);
		computeShader = VK_NULL_HANDLE;
	}

	if (descSetLayouts.size() >= 2)
		vkDestroyDescriptorSetLayout(device, descSetLayouts[1], nullptr);
	descSetLayouts.clear();

	outputImage.reset();
	atmosphereBuffer.reset();
}

void AtmospherePass::Update(double dt)
{
}

void AtmospherePass::Record(const VulkanContext& ctx, const FrameContext& frame)
{
	const VkCommandBuffer cmd = GetCommandBuffer();
	const uint32_t width = outputImage->GetInfo().extent.width;
	const uint32_t height = outputImage->GetInfo().extent.height;

	vkCmdBindPipeline(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	std::array<VkDescriptorSet, 2> descSets = { frame.cameraSet, descSet1 };
	vkCmdBindDescriptorSets(cmd, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, descSets.size(), descSets.data(), 0, nullptr);
	vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(width / 16.f)), static_cast<uint32_t>(std::ceil(height / 16.f)), 1);
}

void AtmospherePass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	APass::SetUsages(ctx, frame);
	AddUsage(outputImage->GetImage(), VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
}

void AtmospherePass::SetAtmosphere(const Atmosphere& atmosphere)
{
	this->atmosphere = atmosphere;
	atmosphereBuffer->SetData(&this->atmosphere);
}

void AtmospherePass::PrepareResource(const VulkanContext& ctx)
{
	glm::vec3 sunDir = glm::normalize(glm::vec3{ 1.f, -1.f, 1.f });
	atmosphere.sun = glm::vec4{ sunDir, 20.f };
	const VkBufferUsageFlags usage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	const VkMemoryPropertyFlags memProps = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	atmosphereBuffer = std::make_unique<VulkanBuffer>(VulkanBuffer::Create(ctx, usage, memProps, sizeof(atmosphere), &atmosphere));

	VkImageCreateInfo imgCi{};
	imgCi.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imgCi.format = VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
	imgCi.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
	imgCi.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
	imgCi.imageType = VkImageType::VK_IMAGE_TYPE_2D;
	imgCi.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT;
	imgCi.extent = { 1024, 768, 1 };
	imgCi.mipLevels = 1;
	imgCi.arrayLayers = 1;
	imgCi.tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL;
	imgCi.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
	outputImage = std::make_unique<VulkanImage>(VulkanImage::Create(ctx, imgCi, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
}

void AtmospherePass::SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool, VkDescriptorSetLayout cameraSetLayout)
{
	const VkDevice device = ctx.GetDevice();

	std::vector<VkDescriptorSetLayoutBinding> set1Bindings;
	VkDescriptorSetLayoutBinding& binding0 = set1Bindings.emplace_back();
	binding0.binding = 0;
	binding0.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	binding0.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding0.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding1 = set1Bindings.emplace_back();
	binding1.binding = 1;
	binding1.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	binding1.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binding1.descriptorCount = 1;

	descSetLayouts.resize(2);
	descSetLayouts[0] = cameraSetLayout;
	VkDescriptorSetLayoutCreateInfo layoutCi{};
	layoutCi.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCi.bindingCount = static_cast<uint32_t>(set1Bindings.size());
	layoutCi.pBindings = set1Bindings.data();
	VK_RESULT_CHECK(vkCreateDescriptorSetLayout(device, &layoutCi, nullptr, &descSetLayouts[1]));

	VkDescriptorSetAllocateInfo descSetAllocInfo{};
	descSetAllocInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descSetAllocInfo.descriptorPool = descPool;
	descSetAllocInfo.pSetLayouts = &descSetLayouts[1];
	descSetAllocInfo.descriptorSetCount = 1;
	VK_RESULT_CHECK(vkAllocateDescriptorSets(device, &descSetAllocInfo, &descSet1));

	VkDescriptorBufferInfo descBufferInfo{};
	descBufferInfo.buffer = atmosphereBuffer->GetBuffer();
	descBufferInfo.range = sizeof(atmosphere);
	VkDescriptorImageInfo descImageInfo{};
	descImageInfo.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL;
	descImageInfo.imageView = outputImage->GetView();

	VkWriteDescriptorSet write0{};
	write0.sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write0.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write0.descriptorCount = 1;
	write0.dstSet = descSet1;
	write0.dstBinding = 0;
	write0.pBufferInfo = &descBufferInfo;
	VkWriteDescriptorSet write1{};
	write1.sType = VkStructureType::VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write1.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write1.descriptorCount = 1;
	write1.dstSet = descSet1;
	write1.dstBinding = 1;
	write1.pImageInfo = &descImageInfo;
	std::array<VkWriteDescriptorSet, 2> writes = { write0 , write1 };
	vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
}

void AtmospherePass::BuildPipeline(const VulkanContext& ctx)
{
	computeShader = LoadShader(ctx.GetDevice(), "shaders/atmosphere.comp.spv");
	VkPipelineShaderStageCreateInfo shaderStageCI{};
	shaderStageCI.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCI.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
	shaderStageCI.module = computeShader;
	shaderStageCI.pName = "main";

	VkPipelineLayoutCreateInfo pipelineLayoutCI{};
	pipelineLayoutCI.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.setLayoutCount = descSetLayouts.size();
	pipelineLayoutCI.pSetLayouts = descSetLayouts.data();
	VK_RESULT_CHECK(vkCreatePipelineLayout(ctx.GetDevice(), &pipelineLayoutCI, nullptr, &pipelineLayout));

	VkComputePipelineCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	ci.layout = pipelineLayout;
	ci.stage = shaderStageCI;
	VK_RESULT_CHECK(vkCreateComputePipelines(ctx.GetDevice(), nullptr, 1, &ci, nullptr, &pipeline));
}
