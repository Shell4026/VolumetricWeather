#pragma once
#include "APass.h"

#include "render/VulkanContext.h"

#include "weather/IWeatherPass.h"

#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"

#include <memory>
class VulkanImage;
class VulkanSampler;
class Material;
class Shader;
class LUTPass : public APass, public IWeatherPass
{
public:
	struct GlobalSetting
	{
		uint32_t transmittanceLUTSteps = 64;
		uint32_t skyViewLUTSteps = 64;
		uint32_t aerialPerspectiveLUTSteps = 5;
		uint32_t aerialShadowSteps = 128;
		uint32_t msLUTSteps = 64;
		float apFactor = 3.2f;
	};
	enum LUTType
	{
		SkyView = 1,
		AerialPerspective = 2,
		Transmittance = 4,
		AerialShadow = 8,
		MultipleScattering = 16,
		All = 31
	};
	using LUTTypeFlags = uint32_t;
public:
	void Clear() override;

	void SetUsages(const FrameContext& frame) override;

	void Record(const FrameContext& frame) override;

	void SetSetting(const WeatherSetting::Atmosphere& atmosphereSetting) override;
	void SetSetting(const WeatherSetting::Lighting& lightingSetting) override;
	void SetSetting(const GlobalSetting& setting);
	
	void SetShadowMap(const VulkanImage& shadowMap) { this->shadowMap = &shadowMap; }
	void SetShadowSampler(const VulkanSampler& sampler) { shadowSampler = &sampler; }
	void SetDepthTexture(const VulkanImage& depthTexture) { depthTex = &depthTexture; }
	void SetNoiseTexture(const VulkanImage& noiseTexture) { noiseTex = &noiseTexture; }

	void EnablePass(LUTTypeFlags flags) { enableLUTFlags |= flags; }
	void DisablePass(LUTTypeFlags flags) { enableLUTFlags &= ~flags; }
	void TogglePass(LUTTypeFlags flags) { enableLUTFlags ^= flags; }

	void UpdateLUTFlags(LUTTypeFlags types) { updateLUTFlags |= types; }
	void ReCreateSkyViewLUT(uint32_t width, uint32_t height);
	void ReCreateShadowLUT(uint32_t width, uint32_t height);

	auto GetSetting() const -> const GlobalSetting& { return setting; }
	auto GetTransmittanceLUTSampler() const -> const VulkanSampler* { return transmittance.sampler; }
	auto GetTransmittanceLUT() const -> VulkanImage* { return transmittance.lut.get(); }
	auto GetSkyViewLUTSampler() const -> const VulkanSampler* { return skyView.sampler; }
	auto GetSkyViewLUT() const -> VulkanImage* { return skyView.lut.get(); }
	auto GetAerialPerspectiveSampler() const -> const VulkanSampler* { return aerialPerspective.sampler; }
	auto GetAerialPerspectiveLUT() const -> VulkanImage* { return aerialPerspective.lut.get(); }
	auto GetAerialShadowSampler() const -> const VulkanSampler* { return aerialPerspective.sampler; } // 같은 샘플러
	auto GetAerialShadowLUT() const -> VulkanImage* { return aerialShadow.lut.get(); }
	auto GetLUTElpasedTimeMs(LUTType type) const -> double;
protected:
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;
	void BuildPipeline(const VulkanContext& ctx) override;
private:
	struct alignas(16) TransmittanceSetting
	{
		uint32_t steps = 32;
	} transmitSetting;
	struct alignas(16) MultipleScatteringSetting
	{
		uint32_t steps = 32;
	} msSetting;
	struct alignas(16) SkyViewSetting
	{
		uint32_t steps = 32;
	} skyViewSetting;
	struct alignas(16) AerialPerspectiveSetting
	{
		uint32_t steps = 4;
		float distanceFactor = 3.2f;
	} aerialSetting;
	struct alignas(16) AerialShadowSetting
	{
		uint32_t steps = 20;
		float apFactor = 3.2f;
	} shadowSetting;

	struct Transmittance
	{
		std::unique_ptr<Shader> shader;
		std::unique_ptr<Material> material;
		std::unique_ptr<VulkanImage> lut;
		const VulkanSampler* sampler = nullptr;
		VkPipeline pipeline = VK_NULL_HANDLE;
		GPUTimer timer;
	} transmittance;
	struct MultipleScattering
	{
		std::unique_ptr<Shader> shader;
		std::unique_ptr<Material> material;
		std::unique_ptr<VulkanImage> lut;
		const VulkanSampler* sampler = nullptr;
		VkPipeline pipeline = VK_NULL_HANDLE;
		GPUTimer timer;
	} multipleScattering;
	struct SkyView
	{
		std::unique_ptr<Shader> shader;
		std::unique_ptr<Material> material;
		std::unique_ptr<VulkanImage> lut;
		const VulkanSampler* sampler = nullptr;
		VkPipeline pipeline = VK_NULL_HANDLE;
		GPUTimer timer;
	} skyView;
	struct AerialPerspective
	{
		std::unique_ptr<Shader> shader;
		std::unique_ptr<Material> material;
		std::unique_ptr<VulkanImage> lut;
		const VulkanSampler* sampler = nullptr;
		VkPipeline pipeline = VK_NULL_HANDLE;
		GPUTimer timer;
	} aerialPerspective;
	struct AerialShadow
	{
		std::unique_ptr<Shader> shader;
		std::unique_ptr<Material> material;
		std::unique_ptr<VulkanImage> lut;
		VkPipeline pipeline = VK_NULL_HANDLE;
		GPUTimer timer;
	} aerialShadow;

	GlobalSetting setting;

	const VulkanImage* shadowMap = nullptr;
	const VulkanSampler* shadowSampler = nullptr;
	const VulkanImage* depthTex = nullptr;
	const VulkanImage* noiseTex = nullptr;
	const VulkanSampler* noiseSampler = nullptr;

	uint32_t updateLUTFlags = 0;
	uint32_t enableLUTFlags = LUTType::All;
};
