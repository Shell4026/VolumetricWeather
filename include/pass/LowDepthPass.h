#pragma once
#include "APass.h"

#include <memory>

class Material;
class Shader;
class LowDepthPass : public APass
{
public:
	void Clear() override;

	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;
	void Record(const VulkanContext& ctx, const FrameContext& frame) override;

	void SetDepthTexture(const VulkanImage& tex);

	auto GetHalfDepthTexture() const -> const VulkanImage* { return halfDepth.get(); }
	auto GetQuarterDepthTexture() const -> const VulkanImage* { return quarterDepth.get(); }
protected:
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;
	void BuildPipeline(const VulkanContext& ctx) override;
private:
	std::unique_ptr<VulkanImage> halfDepth;
	std::unique_ptr<VulkanImage> quarterDepth;

	std::unique_ptr<Material> materialHalf;
	std::unique_ptr<Material> materialQuarter;
	std::unique_ptr<Shader> shader;
	VkPipeline pipeline = VK_NULL_HANDLE;

	const VulkanImage* depthTex = nullptr;

	const VulkanSampler* depthSampler = nullptr;
};