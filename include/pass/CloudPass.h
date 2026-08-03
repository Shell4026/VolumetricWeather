#pragma once
#include "pass/APass.h"

#include "glm/glm.hpp"

#include <memory>

class Shader;
class Material;

class CloudPass : public APass
{
public:
	struct alignas(16) Setting
	{
		uint32_t steps = 80;
		uint32_t lightViewSteps = 60;
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
		float tiling = 96'000.f;
		float extinctionCoefficient = 100.f;
		float coverage = 0.1f;
		float historyWeight = 0.9f;
		alignas(16) glm::vec2 offset{ 0.f };
		alignas(16) glm::vec4 sun{ 0.f };
	};
public:
	void Clear() override;
	void Record(const VulkanContext& ctx, const FrameContext& frame) override;
	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;
	void SetSetting(const Setting& setting);
	void SetSceneDepthTexture(const VulkanImage& img) { sceneDepth = &img; }
	void SetTransmittanceLUT(const VulkanImage& img, const VulkanSampler& sampler) { transmittanceLUT = &img; transmittanceLUTSampler = &sampler; }
	void SetNoise(const VulkanImage& tex) { noiseTex = &tex; }

	auto GetOutputImage() const -> const VulkanImage* { return curOutput; }
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
	std::unique_ptr<VulkanImage> tex1;
	std::unique_ptr<VulkanImage> tex2;
	std::unique_ptr<VulkanImage> cloudDepthHistory1;
	std::unique_ptr<VulkanImage> cloudDepthHistory2;
	std::unique_ptr<Shader> shader;
	std::unique_ptr<Material> material;
	VkPipeline pipeline = VK_NULL_HANDLE;
	const VulkanImage* curOutput = nullptr;
	const VulkanImage* prevOutput = nullptr;
	const VulkanImage* curCloudDepth = nullptr;
	const VulkanImage* prevCloudDepth = nullptr;
	const VulkanImage* noiseTex = nullptr;
	const VulkanImage* sceneDepth = nullptr;
	const VulkanImage* transmittanceLUT = nullptr;
	const VulkanSampler* sampler = nullptr;
	const VulkanSampler* transmittanceLUTSampler = nullptr;
	const VulkanSampler* depthSampler = nullptr;
	const VulkanSampler* pointSampler = nullptr;
	Setting setting;
	struct TRP
	{
		glm::vec3 pos;
		int t = 0;
		alignas(16) glm::mat4 viewProj;
	} trp;
	int time = 0;
	bool bSettingDirty = false;
};