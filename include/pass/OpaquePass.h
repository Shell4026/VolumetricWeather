#pragma once
#include "APass.h"
#include "GLBLoader.h"

#include "render/Shader.h"
#include "render/Drawable.hpp"

#include "glm/glm.hpp"

#include <memory>
#include <vector>
class VulkanBuffer;
class VulkanImage;
class OpaquePass : public APass
{
public:
	void Clear(const VulkanContext& ctx, VkDescriptorPool descPool) override;

	void Record(const VulkanContext& ctx, const FrameContext& frame) override;

	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;
	/// @brief 같은 셰이더만 허용
	void PushDrawable(const Drawable& mesh);

	auto GetShader() const -> const Shader& { return opaqueShader; }
	auto GetOutputImage() const -> VulkanImage* { return outputImage.get(); }
	auto GetOutputImageDepth() const -> VulkanImage* { return outputImageDepth.get(); }
protected:
	void PrepareResource(const VulkanContext& ctx) override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool, VkDescriptorSetLayout cameraSetLayout) override;
	void BuildPipeline(const VulkanContext& ctx) override;
private:
	Shader opaqueShader;
	VkPipeline pipeline = VK_NULL_HANDLE;

	std::unique_ptr<VulkanImage> outputImage;
	std::unique_ptr<VulkanImage> outputImageDepth;

	std::vector<const Drawable*> drawables;
};