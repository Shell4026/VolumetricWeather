#pragma once
#include "APass.h"

#include "render/VulkanContext.h"

#include "glm/vec4.hpp"

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
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
		uint32_t transmittanceLUTSteps = 40;
		uint32_t skyViewLUTSteps = 40;
		uint32_t aerialPerspectiveLUTSteps = 40;
	} globalSetting;
public:
	void Clear() override;

	void Record(const VulkanContext& ctx, const FrameContext& frame) override;

	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;

	void ReCreateSkyViewLUT(uint32_t width, uint32_t height);

	auto GetTransmittanceLUTSampler() const -> VulkanSampler* { return transmittance.sampler.get(); }
	auto GetTransmittanceLUT() const -> VulkanImage* { return transmittance.lut.get(); }
	auto GetSkyViewLUTSampler() const -> VulkanSampler* { return skyView.sampler.get(); }
	auto GetSkyViewLUT() const -> VulkanImage* { return skyView.lut.get(); }
	auto GetAerialPerspectiveSampler() const -> VulkanSampler* { return aerialPerspective.sampler.get(); }
	auto GetAerialPerspectiveLUT() const -> VulkanImage* { return aerialPerspective.lut.get(); }
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
	} transmitSetting;
	struct alignas(16) SkyViewSetting
	{
		glm::vec4 sun;
		uint32_t steps = 40;
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
	} skyViewSetting;
	struct alignas(16) AerialPerspectiveSetting
	{
		glm::vec4 sun;
		uint32_t steps = 40;
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
	} aerialSetting;

	struct Transmittance
	{
		std::unique_ptr<Shader> shader;
		std::unique_ptr<Material> material;
		std::unique_ptr<VulkanImage> lut;
		std::unique_ptr<VulkanSampler> sampler;
		VkPipeline pipeline = VK_NULL_HANDLE;
	} transmittance;
	struct SkyView
	{
		std::unique_ptr<Shader> shader;
		std::unique_ptr<Material> material;
		std::unique_ptr<VulkanImage> lut;
		std::unique_ptr<VulkanSampler> sampler;
		VkPipeline pipeline = VK_NULL_HANDLE;
	} skyView;
	struct AerialPerspective
	{
		std::unique_ptr<Shader> shader;
		std::unique_ptr<Material> material;
		std::unique_ptr<VulkanImage> lut;
		std::unique_ptr<VulkanSampler> sampler;
		VkPipeline pipeline = VK_NULL_HANDLE;
	} aerialPerspective;
};