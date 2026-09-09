#pragma once
#include "VulkanImage.h"

#include <glm/glm.hpp>

#include <memory>
#include <vector>
class Camera;
class VulkanContext;
class VulkanImage;
class CSM
{
public:
	void Create(const VulkanContext& ctx, const Camera& mainCamera, const glm::vec3& lightDir, uint32_t split);
	void Clear();

	auto GetViewMatrix(uint32_t cascade) const -> const glm::mat4& { return matView[cascade]; }
	auto GetProjMatrix(uint32_t cascade) const -> const glm::mat4& { return matProj[cascade]; }
	auto GetViewProjMatrix(uint32_t cascade) const -> const glm::mat4& { return matViewProj[cascade]; }
	auto GetShadowMap() const -> VulkanImage* { return shadowMapArray.get(); }
	auto GetSliceCount() const -> uint32_t { return split; }
	auto GetSliceLength() const -> const std::vector<float>& { return sliceLength; }
private:
	void CreateShadowMap(const VulkanContext& ctx);
private:
	std::vector<glm::mat4> matView;
	std::vector<glm::mat4> matProj;
	std::vector<glm::mat4> matViewProj;
	std::vector<float> sliceLength;
	std::unique_ptr<VulkanImage> shadowMapArray;
	uint32_t split = 0;
};