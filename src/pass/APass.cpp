#include "pass/APass.h"
#include "core/Logger.h"

#include <fstream>

void APass::Init(const VulkanContext& ctx, VkDescriptorPool descPool, VkDescriptorSetLayout cameraSetLayout)
{
	this->cmdPool = ctx.GetCommandPool();
	this->descPool = descPool;

	AllocateCommandBuffer(ctx.GetDevice());
	PrepareResource(ctx);
	SetupDescriptors(ctx, descPool, cameraSetLayout);
	BuildPipeline(ctx);
}

void APass::Clear(const VulkanContext& ctx, VkDescriptorPool descPool)
{
	if (cmd != VK_NULL_HANDLE)
	{
		vkFreeCommandBuffers(ctx.GetDevice(), cmdPool, 1, &cmd);
		cmd = VK_NULL_HANDLE;
	}
}

void APass::BeginRecord(const std::vector<BarrierInfo>* barrierInfos)
{
	const VkCommandBuffer cmd = GetCommandBuffer();
	vkResetCommandBuffer(cmd, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	vkBeginCommandBuffer(cmd, &beginInfo);

	if (barrierInfos != nullptr)
	{
		for (const BarrierInfo& info : *barrierInfos)
		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = info.srcAccess;
			barrier.dstAccessMask = info.dstAccess;
			barrier.oldLayout = info.srcLayout;
			barrier.newLayout = info.dstLayout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange = { info.aspect, 0, 1, 0, 1 };
			barrier.image = info.image;

			vkCmdPipelineBarrier(
				cmd,
				info.srcStage,
				info.dstStage,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);
		}
	}
}

void APass::EndRecord(const std::vector<BarrierInfo>* barrierInfos)
{
	if (barrierInfos != nullptr)
	{
		for (const BarrierInfo& info : *barrierInfos)
		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VkStructureType::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = info.srcAccess;
			barrier.dstAccessMask = info.dstAccess;
			barrier.oldLayout = info.srcLayout;
			barrier.newLayout = info.dstLayout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange = { info.aspect, 0, 1, 0, 1 };
			barrier.image = info.image;

			vkCmdPipelineBarrier(
				cmd,
				info.srcStage,
				info.dstStage,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);
		}
	}
	vkEndCommandBuffer(cmd);
}

void APass::SetUsages(const VulkanContext& ctx, const FrameContext& frame)
{
	imageUsages.clear();
}

void APass::AddUsage(VkImage image, VkImageLayout usage)
{
	ImageUsage imgUsage{};
	imgUsage.image = image;
	imgUsage.layout = usage;
	switch (usage)
	{
	case VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		imgUsage.stage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		imgUsage.access = VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;
	case VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		imgUsage.stage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		imgUsage.access = VkAccessFlagBits::VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		imgUsage.aspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT | VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT;
		break;
	case VkImageLayout::VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		imgUsage.stage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		break;
	case VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		imgUsage.stage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		imgUsage.access = VkAccessFlagBits::VK_ACCESS_SHADER_READ_BIT;
		break;
	case VkImageLayout::VK_IMAGE_LAYOUT_GENERAL:
		imgUsage.stage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		imgUsage.access = VkAccessFlagBits::VK_ACCESS_SHADER_WRITE_BIT;
	}
	imageUsages.push_back(imgUsage);
}

auto APass::LoadShader(VkDevice device, const std::filesystem::path& path) -> VkShaderModule
{
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		SH_ERROR_FORMAT("Failed to read shader: {}", path.string());
		return VK_NULL_HANDLE;
	}
	const std::size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	VkShaderModuleCreateInfo ci{};
	ci.sType = VkStructureType::VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = buffer.size();
	ci.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

	VkShaderModule shader = VK_NULL_HANDLE;
	if (VkResult result = vkCreateShaderModule(device, &ci, nullptr, &shader); result != VkResult::VK_SUCCESS)
	{
		SH_ERROR_FORMAT("Failed to create shader module!: {}", string_VkResult(result));
		throw std::runtime_error("Failed to create shader module!");
	}
	return shader;
}

void APass::AllocateCommandBuffer(VkDevice device)
{
	VkCommandBufferAllocateInfo cmdAllocInfo{};
	cmdAllocInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdAllocInfo.commandPool = cmdPool;
	cmdAllocInfo.commandBufferCount = 1;
	cmdAllocInfo.level = VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VK_RESULT_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd));
}
