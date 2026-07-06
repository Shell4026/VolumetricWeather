#pragma once
#include "VulkanContext.h"
#include "FrameContext.h"
#include "BarrierInfo.h"

#include <filesystem>
#include <vector>
class APass
{
public:
	void Init(const VulkanContext& ctx, VkDescriptorPool descPool, VkDescriptorSetLayout cameraSetLayout);
	virtual void Clear(const VulkanContext& ctx, VkDescriptorPool descPool);
	virtual void Update(double dt) {}

	void BeginRecord(const std::vector<BarrierInfo>* barrierInfos = nullptr);
	virtual void Record(const VulkanContext& ctx, const FrameContext& frame) = 0;
	void EndRecord(const std::vector<BarrierInfo>* barrierInfos = nullptr);

	virtual void SetUsages(const VulkanContext& ctx, const FrameContext& frame);

	auto GetCommandBuffer() const -> VkCommandBuffer { return cmd; }
	auto GetUsages() const -> const std::vector<ImageUsage>& { return imageUsages; }
protected:
	virtual void PrepareResource(const VulkanContext& ctx) {};
	virtual void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool, VkDescriptorSetLayout cameraSetLayout) = 0;
	virtual void BuildPipeline(const VulkanContext& ctx) = 0;
	void AddUsage(VkImage image, VkImageLayout usage);

	static auto LoadShader(VkDevice device, const std::filesystem::path& path) -> VkShaderModule;
private:
	void AllocateCommandBuffer(VkDevice device);
private:
	VkCommandPool cmdPool = VK_NULL_HANDLE;
	VkDescriptorPool descPool = VK_NULL_HANDLE;
	VkCommandBuffer cmd = VK_NULL_HANDLE;

	std::vector<ImageUsage> imageUsages;
};