#pragma once
#include "pass/APass.h"

#include "glm/vec2.hpp"

#include <memory>

class Shader;
class Material;

class CloudPass : public APass
{
public:
	struct alignas(16) Setting
	{
		uint32_t steps = 80;
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
		float tiling = 96'000.f;
		float extinctionCoefficient = 0.0005f;
		float coverage = 0.1f;
		glm::vec2 offset{ 0.f };
		glm::vec4 sun{ 0.f };
	};
public:
	void Clear() override;
	void Record(const VulkanContext& ctx, const FrameContext& frame) override;
	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;
	void SetSetting(const Setting& setting);
	void SetSceneDepthTexture(const VulkanImage& img) { sceneDepth = &img; }
	void SetTransmittanceLUT(const VulkanImage& img, const VulkanSampler& sampler) { transmittanceLUT = &img; transmittanceLUTSampler = &sampler; }

	auto GetOutputImage() const -> VulkanImage* { return output.get(); }
	auto GetSampler() const -> const VulkanSampler* { return sampler; }
	auto GetSetting() const -> const Setting& { return setting; }
protected:
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;
	void BuildPipeline(const VulkanContext& ctx) override;
private:
	void LoadNoises();
private:
	std::unique_ptr<VulkanImage> perlin;
	std::unique_ptr<VulkanImage> output;
	std::unique_ptr<Shader> shader;
	std::unique_ptr<Material> material;
	VkPipeline pipeline = VK_NULL_HANDLE;
	const VulkanImage* sceneDepth = nullptr;
	const VulkanImage* transmittanceLUT = nullptr;
	const VulkanSampler* sampler = nullptr;
	const VulkanSampler* transmittanceLUTSampler = nullptr;
	const VulkanSampler* depthSampler = nullptr;
	Setting setting;
	bool bSettingDirty = false;
};