#pragma once
#include "pass/APass.h"

#include "glm/glm.hpp"

#include <memory>

class Shader;
class Material;
class CloudPass;

class CloudTRPass : public APass
{
public:
	struct alignas(16) Setting
	{
		glm::vec3 pos{ 0.f };
		alignas(16) glm::mat4 viewProj{ 1.f };
		float historyWeight = 0.95f;
		uint32_t historyValid = 0;
	};
public:
	CloudTRPass(const CloudPass& cloudPass);

	void Clear() override;
	void Record(const VulkanContext& ctx, const FrameContext& frame) override;
	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;
	void SetSetting(const Setting& setting);
	void InvalidateHistory();

	auto GetOutputImage() const -> const VulkanImage* { return curOutput; }
	auto GetDepthImage() const -> const VulkanImage* { return curDepth; }
	auto GetSampler() const -> const VulkanSampler* { return sampler; }
	auto GetSetting() const -> const Setting& { return setting; }
protected:
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;
	void BuildPipeline(const VulkanContext& ctx) override;
private:
	void CreateTRShader(VkDescriptorSetLayout cameraSetLayout);
private:
	const CloudPass& cloudPass;

	std::unique_ptr<VulkanImage> output;
	std::unique_ptr<VulkanImage> output2;
	std::unique_ptr<VulkanImage> depth;
	std::unique_ptr<VulkanImage> depth2;
	std::unique_ptr<VulkanImage> accum;
	std::unique_ptr<Shader> shader;
	std::unique_ptr<Material> material;
	VkPipeline pipeline = VK_NULL_HANDLE;

	const VulkanImage* curOutput = nullptr;
	const VulkanImage* curDepth = nullptr;
	const VulkanImage* prevOutput = nullptr;
	const VulkanImage * prevDepth = nullptr;
	const VulkanSampler* sampler = nullptr;

	Setting setting;
	bool historyValid = false;
	uint32_t historyFrameCount = 0;
	uint64_t cloudSettingRevision = 0;
};
