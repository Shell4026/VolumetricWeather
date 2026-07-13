#pragma once
#include "render/VulkanContext.h"
#include "render/FrameContext.h"
#include "render/BarrierInfo.h"
#include "render/GPUTimer.h"

#include <filesystem>
#include <vector>
class APass
{
public:
	virtual ~APass();

	void Init(const VulkanContext& ctx, VkDescriptorPool descPool, VkDescriptorSetLayout cameraSetLayout);
	virtual void Clear();
	virtual void Update(double dt) {}

	void BeginRecord(const std::vector<BarrierInfo>* barrierInfos = nullptr);
	virtual void Record(const VulkanContext& ctx, const FrameContext& frame) = 0;
	void EndRecord(const std::vector<BarrierInfo>* barrierInfos = nullptr);

	virtual void SetUsages(const VulkanContext& ctx, const FrameContext& frame);

	void UseTimer(bool bUse) { bUseTimer = bUse; }

	/// @brief 타이머 사용 안 하면 0 반환
	auto GetElapsedTimeMs() const -> double;
	auto GetCommandBuffer() const -> VkCommandBuffer { return cmd; }
	auto GetUsages() const -> const std::vector<ImageUsage>& { return imageUsages; }
	auto IsUsingSwapchainImage() const -> bool { return bUseSwapchainImage; }
	auto UsingTimer() const -> bool { return bUseTimer; }
protected:
	virtual void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) {};
	virtual void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) {}
	virtual void BuildPipeline(const VulkanContext& ctx) = 0;

	void AddUsage(VkImage image, VkImageAspectFlags apsect, VkImageLayout usage);

	static auto LoadShader(VkDevice device, const std::filesystem::path& path) -> VkShaderModule;
private:
	void AllocateCommandBuffer(VkDevice device);
protected:
	const VulkanContext* ctx = nullptr;
	VkDescriptorPool descPool = VK_NULL_HANDLE;
	bool bUseSwapchainImage = true;
private:
	VkCommandPool cmdPool = VK_NULL_HANDLE;
	VkCommandBuffer cmd = VK_NULL_HANDLE;

	std::vector<ImageUsage> imageUsages;

	GPUTimer timer;
	bool bUseTimer = true;
};