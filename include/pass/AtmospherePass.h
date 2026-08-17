#pragma once
#include "APass.h"
#include "render/VulkanImage.h"

#include "weather/Setting.h"
#include "weather/IWeatherPass.h"

#include <glm/glm.hpp>

#include <vector>
#include <memory>

class Shader;
class Material;

class AtmosphereBasePass : public APass, public IWeatherPass
{
public:
	AtmosphereBasePass() { bUseSwapchainImage = false; }
	~AtmosphereBasePass();

	void Clear() override;
	void Record(const FrameContext& frame) override;
	void SetUsages(const FrameContext& frame) override;
	void SetOpaqueDepthTexture(const VulkanImage& opaqueDepthTex) { this->opaqueDepthTex = &opaqueDepthTex; }
	void SetOpaqueTexture(const VulkanImage& opaqueDepthTex) { this->opaqueTex = &opaqueDepthTex; }
	void SetShadowMap(const VulkanImage& shadowMap) { this->shadowMap = &shadowMap; }
	void SetShadowSampler(const VulkanSampler& sampler) { shadowSampler = &sampler; }
	void SetImageSize(uint32_t width, uint32_t height) { this->width = width; this->height = height; }

	auto GetOutputImage() const -> VulkanImage* { return outputImage.get(); }
	auto GetShader() const -> Shader* { return computeShader.get(); }
protected:
	virtual auto CreateShader(VkDevice device, VkDescriptorSetLayout cameraSetLayout) -> Shader = 0;
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;
	void BuildPipeline(const VulkanContext& ctx) override;
protected:
	std::unique_ptr<Material> material;
	VkPipeline pipeline = VK_NULL_HANDLE;

	std::unique_ptr<VulkanImage> outputImage;

	const VulkanImage* opaqueDepthTex = nullptr;
	const VulkanImage* opaqueTex = nullptr;
	const VulkanImage* shadowMap = nullptr;
	const VulkanSampler* opaqueSampler = nullptr;
	const VulkanSampler* opaqueDepthSampler = nullptr;
	const VulkanSampler* shadowSampler = nullptr;
private:
	VkDescriptorSetLayout cameraSetLayout = VK_NULL_HANDLE;

	std::unique_ptr<Shader> computeShader;

	uint32_t width = 1024;
	uint32_t height = 768;
};

class AtmospherePass : public AtmosphereBasePass
{
public:
	struct alignas(16) Setting
	{
		glm::ivec2 steps = { 64, 20 };
	};
public:
	void SetSetting(const WeatherSetting::Atmosphere& atmosphereSetting) override;
	void SetSetting(const WeatherSetting::Lighting& lightingSetting) override;
	void SetSetting(const Setting& setting);
	auto GetSetting() const -> const Setting& { return setting; }
protected:
	auto CreateShader(VkDevice device, VkDescriptorSetLayout cameraSetLayout) -> Shader override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;
private:
	Setting setting;
};
