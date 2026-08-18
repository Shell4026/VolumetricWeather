#pragma once
#include "pass/APass.h"
#include "Bezier.hpp"

#include "weather/IWeatherPass.h"

#include <glm/glm.hpp>

#include <memory>
#include <vector>
class Shader;
class Material;

class CloudPass : public APass, public IWeatherPass
{
public:
	struct Setting
	{
		uint32_t minSteps = 10;
		uint32_t maxSteps = 64;
		uint32_t lightViewSteps = 6;
		uint32_t modeFlags = 1;
		
		uint32_t frame = 0;
		float time = 0.0f;
		float tiling = 90'000.f;
		float tiling2 = 3'000.f;

		float extinctionCoefficient = 200.f;
		float coverage = 0.05f;
		float powderStrength = 0.5f;
		float anvilBias = 0.0f;
		
		Bezier brightnessCurve;

		float brightnessStrength = 1.0f;
		float densityPMin = 0.3f;
		float densityPFactor = 0.8f;
		char padding0[4];

		glm::vec2 windVelKmh{ 1.f, 0.f };
		char padding1[2];

		glm::vec4 cloudColor{ 1.f, 1.f, 1.f, 1.f };
	};
	enum ModeFlag
	{
		LightViewDistanceLimit = 1
	};
public:
	void Clear() override;

	void SetUsages(const FrameContext& frame) override;

	void BeginRecord(const FrameContext& frame, const std::vector<BarrierInfo>* barrierInfos) override;
	void Record(const FrameContext& frame) override;
	
	void SetSetting(const WeatherSetting::Atmosphere& atmosphereSetting) override;
	void SetSetting(const WeatherSetting::Lighting& lightingSetting) override;
	void SetSetting(const Setting& setting);
	void SetSceneDepthTexture(const VulkanImage& img) { sceneDepth = &img; }
	void SetTransmittanceLUT(const VulkanImage& img, const VulkanSampler& sampler) { transmittanceLUT = &img; transmittanceLUTSampler = &sampler; }
	void SetNoise(const VulkanImage& tex) { noiseTex = &tex; }
	void SetCloudMask(const VulkanImage& tex) { cloudMask = &tex; }

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
	const VulkanImage* cloudMask = nullptr;

	const VulkanSampler* sampler = nullptr;
	const VulkanSampler* transmittanceLUTSampler = nullptr;
	const VulkanSampler* depthSampler = nullptr;
	const VulkanSampler* pointSampler = nullptr;
	const VulkanSampler* maskSampler = nullptr;

	Setting setting;
	uint64_t settingRevision = 0;

	uint32_t frameIdx = 0;
	float time = 0.f;
};
