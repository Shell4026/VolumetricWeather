#pragma once
#include "APass.h"

#include <glm/glm.hpp>

class Shader;
class Material;
class VulkanImage;
#include <memory>
class CloudPaintPass : public APass
{
public:
	struct alignas(16) PushConstant
	{
		glm::vec2 brushUV{ 0.f, 0.f };
		uint32_t radius = 0;
	} pc;
public:
	CloudPaintPass();
	~CloudPaintPass();

	void Clear() override;
	void Record(const VulkanContext& ctx, const FrameContext& frame) override;
	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;

	void SetCanvas(const VulkanImage& img) { canvasImagePtr = &img; }
	auto GetCanvas() const -> const VulkanImage* { return canvasImagePtr; }
protected:
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;
	void BuildPipeline(const VulkanContext& ctx) override;
private:
	std::unique_ptr<Shader> shader;
	std::unique_ptr<Material> material;
	VkPipeline pipeline = VK_NULL_HANDLE;

	const VulkanImage* canvasImagePtr = nullptr;
};