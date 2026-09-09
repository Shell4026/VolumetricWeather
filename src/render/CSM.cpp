#include "render/CSM.h"
#include "render/Camera.h"
#include "render/VulkanImage.h"

#include <glm/gtc/matrix_transform.hpp>

#include <limits>

#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
void CSM::Create(const VulkanContext& ctx, const Camera& mainCamera, const glm::vec3& lightDir, uint32_t split)
{
	if (this->split != split)
	{
		this->split = split;
		matView.resize(split);
		matProj.resize(split);
		matViewProj.resize(split);
		sliceLength.resize(split);
		CreateShadowMap(ctx);
	}

	glm::vec3 frustrum[8] =
	{
		{-1.f, 1.f, 0.f}, {1.f, 1.f, 0.f}, {1.f, -1.f, 0.f}, {-1.f, -1.f, 0.f},
		{-1.f, 1.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, -1.f, 1.f}, {-1.f, -1.f, 1.f},
	};

	const glm::mat4 invViewProj = mainCamera.GetMatrixInverseView() * mainCamera.GetMatrixInverseProj();
	for (int i = 0; i < 8; ++i)
	{
		glm::vec4 worldPos = invViewProj * glm::vec4{ frustrum[i], 1.f };
		worldPos /= worldPos.w;
		frustrum[i] = worldPos;
	}

	const float near = mainCamera.GetNear();
	const float far = glm::min(mainCamera.GetFar(), 100'000.f);
	for (int i = 0; i < split; ++i)
	{
		const float p = static_cast<float>(i + 1) / static_cast<float>(split);
		const float logSplit = near * std::pow(far / near, p);
		const float linearSplit = near + (far - near) * p;
		sliceLength[i] = glm::mix(linearSplit, logSplit, 0.9f);

		const float splitNear = (i == 0) ? near : sliceLength[i - 1];
		const float splitFar = sliceLength[i];

		const float t0 = (splitNear - near) / (far - near);
		const float t1 = (splitFar - near) / (far - near);

		glm::vec3 splitFrustrum[8];
		for (int edge = 0; edge < 4; ++edge)
		{
			splitFrustrum[edge] = glm::mix(frustrum[edge], frustrum[edge + 4], t0);
			splitFrustrum[edge + 4] = glm::mix(frustrum[edge], frustrum[edge + 4], t1);
		}

		glm::vec3 center{ 0.f };
		for (int j = 0; j < 8; ++j)
			center += splitFrustrum[j];
		center /= 8.f;

		float radius = 0.f;
		for (int j = 0; j < 8; ++j)
		{
			const float len = glm::length(splitFrustrum[j] - center);
			radius = std::max(radius, len);
		}
		const float margin = radius;

		const glm::vec3 up = glm::abs(glm::dot(lightDir, glm::vec3(0, 1, 0))) > 0.99f ? glm::vec3{ 0.f, 0.f, 1.f } : glm::vec3{ 0.f, 1.f, 0.f };
		matView[i] = glm::lookAtRH(center - lightDir * (radius + margin), center, up);
		matProj[i] = glm::orthoRH_ZO(-radius, radius, -radius, radius, 0.01f, 2.f * (radius + margin));
		matViewProj[i] = matProj[i] * matView[i];
	}
}

void CSM::Clear()
{
	split = 0;
	matView.clear();
	matProj.clear();
	shadowMapArray.reset();
}

void CSM::CreateShadowMap(const VulkanContext& ctx)
{
	VkImageCreateInfo ci = VulkanImage::GetCreateInfo();
	ci.arrayLayers = split;
	ci.extent = { 4096, 4096, 1 };
	ci.format = VkFormat::VK_FORMAT_D32_SFLOAT;
	ci.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	shadowMapArray = std::make_unique<VulkanImage>(ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}
