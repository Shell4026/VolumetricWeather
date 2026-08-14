#pragma once
#include "render/VulkanContext.h"
#include "render/FrameContext.h"
#include "render/BarrierInfo.h"
#include "render/GPUTimer.h"
#include "render/SamplerManager.h"

#include <filesystem>
#include <vector>
class APass
{
public:
	virtual ~APass();

	void Init(const VulkanContext& ctx, SamplerManager& samplerManager, VkDescriptorPool descPool, VkDescriptorSetLayout cameraSetLayout);
	virtual void Clear();
	virtual void Update(double dt) {}

	virtual void SetUsages(const FrameContext& frame);

	virtual void BeginRecord(const FrameContext& frame, const std::vector<BarrierInfo>* barrierInfos = nullptr);
	virtual void Record(const FrameContext& frame) = 0;
	virtual void EndRecord(const FrameContext& frame, const std::vector<BarrierInfo>* barrierInfos = nullptr);

	void UseTimer(bool bUse) { bUseTimer = bUse; }
	void CreateFence();

	/// @brief 타이머 사용 안 하면 0 반환
	auto GetElapsedTimeMs() const -> double;
	auto GetCommandBuffer() const -> VkCommandBuffer { return cmd; }
	auto GetUsages() const -> const std::vector<ImageUsage>& { return imageUsages; }
	auto IsUsingSwapchainImage() const -> bool { return bUseSwapchainImage; }
	auto UsingTimer() const -> bool { return bUseTimer; }
	auto GetFence() const -> VkFence { return submitCompleted; }
	auto GetSubmitInfo() const -> VkSubmitInfo { return submitInfo; }
protected:
	virtual void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) {};
	virtual void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) {}
	virtual void BuildPipeline(const VulkanContext& ctx) = 0;

	void AddUsage(VkImage image, VkImageAspectFlags apsect, VkImageLayout usage);

	static void AddDescSetLayoutBinding(std::vector<VkDescriptorSetLayoutBinding>& bindings, uint32_t bindingNumber, VkDescriptorType descType, VkShaderStageFlags stage);

	static auto LoadShader(VkDevice device, const std::filesystem::path& path) -> VkShaderModule;
private:
	void AllocateCommandBuffer(VkDevice device);
protected:
	const VulkanContext* ctx = nullptr;
	SamplerManager* samplerManager = nullptr;

	VkDescriptorPool descPool = VK_NULL_HANDLE;
	VkSubmitInfo submitInfo{};

	bool bUseSwapchainImage = true;
private:
	VkCommandPool cmdPool = VK_NULL_HANDLE;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence submitCompleted = VK_NULL_HANDLE;

	std::vector<ImageUsage> imageUsages;

	GPUTimer timer;
	bool bUseTimer = true;
};