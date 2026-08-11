#pragma once
#include "pass/APass.h"

#include "glm/glm.hpp"

#include <memory>
#include <vector>
class Shader;
class Material;

class CloudPass : public APass
{
public:
	struct Setting
	{
		uint32_t steps = 20;
		uint32_t lightViewSteps = 6;
		uint32_t modeFlags = 1;
		uint32_t frame = 0;

		float time = 0.0f;
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
		float tiling = 96'000.f;

		float tiling2 = 3'000.f;
		float extinctionCoefficient = 200.f;
		float coverage = 0.05f;
		float powderStrength = 0.5f;

		float anvilBias = 0.0f;
		float darkHeight = 0.8f;
		float darkStrength = 1.5f;
		float densityPMin = 0.3f;

		float densityPFactor = 0.8f;
		alignas(8) glm::vec2 windVelKmh{ 1.f, 0.f };

		alignas(16) glm::vec4 sun{ 0.f };
	};
	enum ModeFlag
	{
		LightViewDistanceLimit = 1
	};
public:
	void Clear() override;
	void Record(const VulkanContext& ctx, const FrameContext& frame) override;
	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;
	void SetSetting(const Setting& setting);
	void SetSceneDepthTexture(const VulkanImage& img) { sceneDepth = &img; }
	void SetTransmittanceLUT(const VulkanImage& img, const VulkanSampler& sampler) { transmittanceLUT = &img; transmittanceLUTSampler = &sampler; }
	void SetNoise(const VulkanImage& tex) { noiseTex = &tex; }

	auto GetOutputImage() const -> const VulkanImage* { return output.get(); }
	auto GetDepthImage() const -> const VulkanImage* { return depth.get(); }
	auto GetSampler() const -> const VulkanSampler* { return sampler; }
	auto GetSetting() const -> const Setting& { return setting; }
	auto GetSettingRevision() const -> uint64_t { return settingRevision; }
protected:
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;
	void BuildPipeline(const VulkanContext& ctx) override;
private:
	void CreateCloudShader(VkDescriptorSetLayout cameraSetLayout);
	void LoadNoises();
	auto CreateNextNoiseMip(const std::vector<uint8_t>& noise, uint32_t width, uint32_t height, uint32_t depth) -> std::vector<uint8_t>;
private:
	std::unique_ptr<VulkanImage> output;
	std::unique_ptr<VulkanImage> depth;
	std::unique_ptr<VulkanImage> perlin;
	std::unique_ptr<Shader> shader;
	std::unique_ptr<Material> material;
	VkPipeline pipeline = VK_NULL_HANDLE;

	const VulkanImage* noiseTex = nullptr;
	const VulkanImage* sceneDepth = nullptr;
	const VulkanImage* transmittanceLUT = nullptr;

	const VulkanSampler* sampler = nullptr;
	const VulkanSampler* transmittanceLUTSampler = nullptr;
	const VulkanSampler* depthSampler = nullptr;
	const VulkanSampler* pointSampler = nullptr;

	Setting setting;
	uint64_t settingRevision = 0;

	uint32_t frameIdx = 0;
	float time = 0.f;
};
