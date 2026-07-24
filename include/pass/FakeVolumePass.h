#pragma once
#include "APass.h"
#include "IDrawablePass.h"

#include "render/Shader.h"
#include "render/Drawable.hpp"

#include "Camera.h"

#include <memory>
#include <vector>
class VulkanImage;
class VulkanSampler;
class Material;
class FakeVolumePass : public APass, public IDrawablePass
{
public:
	FakeVolumePass();
	~FakeVolumePass();

	void Clear() override;

	void Record(const VulkanContext& ctx, const FrameContext& frame) override;

	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;

	/// @brief 넣으면 FakeVolume셰이더로 렌더링 함
	void PushDrawable(const Drawable& mesh) override { drawables.push_back(&mesh); }

	auto GetDepthTex() const -> VulkanImage* { return depthTex.get(); }
protected:
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;

	void BuildPipeline(const VulkanContext& ctx) override;
private:
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkDescriptorSet emptyDescSet = VK_NULL_HANDLE;

	std::unique_ptr<VulkanImage> depthTex;

	Shader shader;

	std::vector<const Drawable*> drawables;
};