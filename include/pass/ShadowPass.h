#pragma once
#include "APass.h"
#include "IDrawablePass.h"

#include "render/Shader.h"
#include "render/Drawable.hpp"
#include "render/Camera.h"

#include <memory>
#include <vector>
class VulkanImage;
class VulkanSampler;
class Material;
class ShadowPass : public APass, public IDrawablePass
{
public:
	ShadowPass();
	~ShadowPass();

	void Clear() override;
	
	void SetUsages(const FrameContext& frame) override;

	void BeginRecord(const FrameContext& frame, const std::vector<BarrierInfo>* barrierInfos = nullptr) override;
	void Record(const FrameContext& frame) override;
	
	/// @brief 넣으면 shadow셰이더로 렌더링 함
	void PushDrawable(const Drawable& mesh) override { drawables.push_back(&mesh); }

	void SetCamera(const Camera& camera) { cam = camera; }

	auto GetShadowMap() const -> VulkanImage* { return sunShadowMap.get(); }
	auto GetShadowSampler() const -> const VulkanSampler* { return shadowSampler; }
	auto GetCamera() const -> const Camera& { return cam; }
protected:
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;

	void BuildPipeline(const VulkanContext& ctx) override;
private:
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkDescriptorSet emptyDescSet = VK_NULL_HANDLE;

	std::unique_ptr<VulkanImage> sunShadowMap;
	const VulkanSampler* shadowSampler = nullptr;

	Shader shader;
	std::unique_ptr<Material> material;

	Camera cam;

	std::vector<const Drawable*> drawables;

	struct UniformData
	{
		alignas(16) glm::vec3 pos{ 0.f };
		alignas(16) glm::mat4 viewProj{ 1.f };
	} uniformData;
};