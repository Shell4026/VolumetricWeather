#pragma once
#include "APass.h"
#include "render/Shader.h"

#include "glm/glm.hpp"

#include <memory>

class VulkanBuffer;
class VulkanImage;
class VulkanSampler;
class Material;
class PostProcessPass : public APass
{
public:
	PostProcessPass(const VulkanImage& outputImage);

	void Clear() override;

	void SetUsages(const FrameContext& frame) override;

	void Record(const FrameContext& frame) override;
	
	void SetExposure(float exposure);
	void SetOutputImage(const VulkanImage& outputImage);

	auto GetShader() const -> const Shader& { return shader; }
	auto GetExposure() const -> float { return data.exposure; }
protected:
	void PrepareResource(const VulkanContext& ctx, VkDescriptorSetLayout cameraSetLayout) override;
	void BuildPipeline(const VulkanContext& ctx) override;
private:
	static constexpr uint32_t DATA_BINDING = 1;
	Shader shader;
	std::unique_ptr<Material> material;
	VkPipeline pipeline = VK_NULL_HANDLE;

	const VulkanImage* outputImage = nullptr;
	const VulkanSampler* sampler = nullptr;

	struct Data
	{
		float exposure = 1.f;
	} data;
};