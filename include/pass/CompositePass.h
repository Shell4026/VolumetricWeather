#pragma once
#include "APass.h"

#include "glm/glm.hpp"

#include <memory>

class VulkanBuffer;
class VulkanImage;
class CompositePass : public APass
{
public:
	CompositePass(const VulkanImage& atmosphereTex);

	void Clear(const VulkanContext& ctx, VkDescriptorPool descPool) override;

	void Record(const VulkanContext& ctx, const FrameContext& frame) override;

	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;

	auto GetPipelineLayout() const -> VkPipelineLayout { return pipelineLayout; }
	auto GetDescriptorSetLayouts() const -> const std::vector<VkDescriptorSetLayout>& { return descSetLayouts; }
protected:
	void PrepareResource(const VulkanContext& ctx) override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool, VkDescriptorSetLayout cameraSetLayout) override;
	void BuildPipeline(const VulkanContext& ctx) override;
private:
	VkShaderModule vertShader = VK_NULL_HANDLE;
	VkShaderModule fragShader = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSetLayout> descSetLayouts;
	VkDescriptorSet descSet = VK_NULL_HANDLE;

	glm::vec4 color{ 0.f, 1.f, 0.f, 1.f };
	std::unique_ptr<VulkanBuffer> buffer;

	const VulkanImage& atmosphereTex;
	VkSampler atmosphereSampler = VK_NULL_HANDLE;
};