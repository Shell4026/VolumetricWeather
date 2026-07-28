#pragma once
#include "pass/APass.h"

#include <memory>

class Shader;
class Material;

class CloudPass : public APass
{
public:
	struct alignas(16) Setting
	{
		uint32_t steps = 60;
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
		float tiling = 32'000.f;
		float extinctionCoefficient = 0.0001f;
		float coverage = 0.5f;
	};
public:
	void Clear() override;
	void Record(const VulkanContext& ctx, const FrameContext& frame) override;
	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;
	void SetSetting(const Setting& setting);
	void SetSceneDepthTexture(const VulkanImage& img) { sceneDepth = &img; }

	auto GetOutputImage() const -> VulkanImage* { return output.get(); }
	auto GetSetting() const -> const Setting& { return setting; }
protected:
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;
	void BuildPipeline(const VulkanContext& ctx) override;
private:
	void LoadNoises();
private:
	std::unique_ptr<VulkanImage> perlin;
	std::unique_ptr<VulkanImage> worley;
	std::unique_ptr<VulkanImage> output;
	std::unique_ptr<Shader> shader;
	std::unique_ptr<Material> material;
	VkPipeline pipeline = VK_NULL_HANDLE;
	const VulkanImage* sceneDepth = nullptr;
	const VulkanSampler* sampler = nullptr;
	const VulkanSampler* depthSampler = nullptr;
	Setting setting;
	bool bSettingDirty = false;
};