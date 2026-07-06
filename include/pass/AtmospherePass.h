#pragma once
#include "APass.h"

#include "glm/glm.hpp"

#include <vector>
#include <memory>

class VulkanBuffer;
class VulkanImage;

class AtmospherePass : public APass
{
public:
	struct Atmosphere
	{
		alignas(16) glm::vec4 sun; // dir, illuminance
	};
public:
	void Clear(const VulkanContext& ctx, VkDescriptorPool descPool) override;
	void Record(const VulkanContext& ctx, const FrameContext& frame) override;
	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;

	auto GetOutputImage() const -> VulkanImage* { return outputImage.get(); }
protected:
	void PrepareResource(const VulkanContext& ctx) override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool, VkDescriptorSetLayout cameraSetLayout) override;
	void BuildPipeline(const VulkanContext& ctx) override;
private:
	std::vector<VkDescriptorSetLayout> descSetLayouts;
	VkDescriptorSet descSet1 = VK_NULL_HANDLE;

	VkShaderModule computeShader = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	Atmosphere atmosphere;
	std::unique_ptr<VulkanBuffer> atmosphereBuffer;
	std::unique_ptr<VulkanImage> outputImage;
};