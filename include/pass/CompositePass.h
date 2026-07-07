#pragma once
#include "APass.h"
#include "Shader.h"

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

	auto GetShader() const -> const Shader& { return compositeShader; }
protected:
	void PrepareResource(const VulkanContext& ctx) override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool, VkDescriptorSetLayout cameraSetLayout) override;
	void BuildPipeline(const VulkanContext& ctx) override;
private:
	Shader compositeShader;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkDescriptorSet descSet = VK_NULL_HANDLE;

	glm::vec4 color{ 0.f, 1.f, 0.f, 1.f };
	std::unique_ptr<VulkanBuffer> buffer;

	const VulkanImage& atmosphereTex;
	VkSampler atmosphereSampler = VK_NULL_HANDLE;
};