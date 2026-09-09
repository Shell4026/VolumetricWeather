#pragma once
#include "APass.h"
#include "IDrawablePass.h"

#include "render/Shader.h"
#include "render/Drawable.hpp"
#include "render/CSM.h"

#include <memory>
#include <vector>
class VulkanImage;
class VulkanSampler;
class Material;
class Camera;
class CSMPass : public APass, public IDrawablePass
{
public:
	CSMPass();
	~CSMPass();

	void Clear() override;
	
	void SetUsages(const FrameContext& frame) override;

	void Record(const FrameContext& frame) override;
	
	/// @brief 넣으면 csm셰이더로 렌더링 함
	void PushDrawable(const Drawable& mesh) override { drawables.push_back(&mesh); }

	void UpdateCSM(const Camera& mainCamera, const glm::vec3& lightDir);

	auto GetShadowMap() const -> VulkanImage* { return csm.GetShadowMap(); }
	auto GetShadowSampler() const -> const VulkanSampler* { return shadowSampler; }
	auto GetCSM() const -> const CSM& { return csm; }
protected:
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;

	void BuildPipeline(const VulkanContext& ctx) override;
private:
	VkPipeline pipeline = VK_NULL_HANDLE;

	const VulkanSampler* shadowSampler = nullptr;

	Shader shader;

	CSM csm;

	std::vector<const Drawable*> drawables;

	struct alignas(16) UniformData
	{
		glm::mat4 viewProj{ 1.f };
	} uniformData;
};