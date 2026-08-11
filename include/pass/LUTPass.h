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
		uint32_t transmittanceLUTSteps = 64;
		uint32_t skyViewLUTSteps = 64;
		uint32_t aerialPerspectiveLUTSteps = 5;
		uint32_t aerialShadowSteps = 64;
	} globalSetting;
	enum LUTType
	{
		SkyView = 0b0001,
		AerialPerspective = 0b0010,
		Transmittance = 0b0100,
		AerialShadow = 0b1000,
		All = 0b1111
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
	void UseLightShadow(bool b) { bUseLightShadow = b; }

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
	auto IsUseLightShadow() const -> bool { return bUseLightShadow; }
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
	} aerialSetting;
	struct alignas(16) AerialShadowSetting
	{
		glm::vec4 sun;
		glm::mat4 sunViewProj;
		uint32_t steps = 20;
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
		float mieCoefficient = 1.f;

		float mieG = 0.8f;
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
		std::unique_ptr<Shader> shader2;
		std::unique_ptr<Material> material;
		std::unique_ptr<Material> material2;
		std::unique_ptr<VulkanImage> lut;
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkPipeline pipeline2 = VK_NULL_HANDLE;
		GPUTimer timer;
	} aerialShadow;

	const VulkanImage* shadowMap = nullptr;
	const VulkanSampler* shadowSampler = nullptr;
	const VulkanImage* depthTex = nullptr;
	const VulkanImage* noiseTex = nullptr;
	const VulkanSampler* noiseSampler = nullptr;

	uint32_t updateLUTFlags = 0;
	uint32_t enableLUTFlags = LUTType::All;
	
	bool bUseLightShadow = false;
};