#pragma once
#include "APass.h"

#include "render/VulkanContext.h"

#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"

#include <memory>
class VulkanImage;
class VulkanSampler;
class Material;
class Shader;
class LUTPass : public APass
{
public:
	struct GlobalSetting
	{
		glm::vec4 sun;
		glm::mat4 sunViewProj;
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
		float mieCoefficient = 1.f;
		float mieG = 0.8f;
		float apDistanceFactor = 3.2f;
		uint32_t transmittanceLUTSteps = 64;
		uint32_t skyViewLUTSteps = 64;
		uint32_t aerialPerspectiveLUTSteps = 5;
		uint32_t aerialShadowSteps = 64;
		uint32_t msLUTSteps = 64;
	} globalSetting;
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

	void Record(const VulkanContext& ctx, const FrameContext& frame) override;

	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;
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
	void UpdateMaterials();
private:
	struct alignas(16) TransmittanceSetting
	{
		uint32_t steps = 40;
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
		float mieCoefficient = 1.f;
	} transmitSetting;
	struct alignas(16) MultipleScatteringSetting
	{
		uint32_t steps = 32;
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
		float mieCoefficient = 1.f;
	} msSetting;
	struct alignas(16) SkyViewSetting
	{
		glm::vec4 sun;
		uint32_t steps = 40;
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
		float mieCoefficient = 1.f;

		float mieG = 0.8f;
	} skyViewSetting;
	struct alignas(16) AerialPerspectiveSetting
	{
		glm::vec4 sun;
		glm::mat4 sunViewProj;
		uint32_t steps = 20;
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
		float mieCoefficient = 1.f;

		float mieG = 0.8f;
		float distanceFactor = 3.2f;
	} aerialSetting;
	struct alignas(16) AerialShadowSetting
	{
		glm::vec4 sun;
		glm::mat4 sunViewProj;

		uint32_t steps = 20;
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
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

	const VulkanImage* shadowMap = nullptr;
	const VulkanSampler* shadowSampler = nullptr;
	const VulkanImage* depthTex = nullptr;
	const VulkanImage* noiseTex = nullptr;
	const VulkanSampler* noiseSampler = nullptr;

	uint32_t updateLUTFlags = 0;
	uint32_t enableLUTFlags = LUTType::All;
};
