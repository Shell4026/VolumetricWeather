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
	struct alignas(16) Setting
	{
		int steps = 40;
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
	};
	struct alignas(16) SkyViewSetting
	{
		glm::vec4 sun;
		uint32_t steps = 40;
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
	};
public:
	void Clear() override;

	void Record(const VulkanContext& ctx, const FrameContext& frame) override;

	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;

	auto GetTransmittanceLUTSampler() const -> VulkanSampler* { return transmittance.sampler.get(); }
	auto GetTransmittanceLUT() const -> VulkanImage* { return transmittance.lut.get(); }
	auto GetTransmittanceSetting() -> Setting& { return setting; }
	auto GetTransmittanceSetting() const -> const Setting& { return setting; }
	auto GetSkyViewLUTSampler() const -> VulkanSampler* { return skyView.sampler.get(); }
	auto GetSkyViewLUT() const -> VulkanImage* { return skyView.lut.get(); }
	auto GetSkyViewSetting() -> SkyViewSetting& { return skyViewSetting; }
	auto GetSkyViewSetting() const -> const SkyViewSetting& { return skyViewSetting; }
protected:
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;
	void BuildPipeline(const VulkanContext& ctx) override;
private:
	Setting setting;
	SkyViewSetting skyViewSetting;

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
};