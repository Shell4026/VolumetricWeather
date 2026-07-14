#pragma once
#include "APass.h"
#include "render/Shader.h"
#include "render/VulkanImage.h"

#include "glm/glm.hpp"

#include <vector>
#include <memory>

class Material;
class AtmospherePass : public APass
{
public:
	struct Atmosphere
	{
		alignas(16) glm::vec4 sun; // dir, illuminance
		alignas(16) glm::mat4 sunViewProj{ 1.f };
		alignas(8) glm::ivec2 steps = { 64, 20 };
		alignas(4) float radius = 6'460'000.f;
	};
public:
	AtmospherePass() { bUseSwapchainImage = false; }
	~AtmospherePass();

	void Clear() override;
	void Record(const VulkanContext& ctx, const FrameContext& frame) override;
	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;
	void SetAtmosphere(const Atmosphere& atmosphere);
	void SetOpaqueDepthTexture(const VulkanImage& opaqueDepthTex) { this->opaqueDepthTex = &opaqueDepthTex; }
	void SetOpaqueTexture(const VulkanImage& opaqueDepthTex) { this->opaqueTex = &opaqueDepthTex; }
	void SetShadowMap(const VulkanImage& shadowMap) { this->shadowMap = &shadowMap; }
	void SetShadowSampler(const VulkanSampler& sampler) { shadowSampler = &sampler; }

	auto GetOutputImage() const -> VulkanImage* { return outputImage.get(); }
	auto GetAtmosphere() const -> const Atmosphere& { return atmosphere; }
protected:
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;
	void BuildPipeline(const VulkanContext& ctx) override;
private:
	VkDescriptorSetLayout cameraSetLayout = VK_NULL_HANDLE;
	std::unique_ptr<Material> material;
	std::vector<VkDescriptorSetLayout> descSetLayouts;

	Shader computeShader;
	VkPipeline pipeline = VK_NULL_HANDLE;

	Atmosphere atmosphere;
	std::unique_ptr<VulkanImage> outputImage;

	const VulkanImage* opaqueDepthTex = nullptr;
	const VulkanImage* opaqueTex = nullptr;
	const VulkanImage* shadowMap = nullptr;
	VulkanSampler opaqueSampler;
	const VulkanSampler* shadowSampler = nullptr;
};