#pragma once
#include "AtmospherePass.h"
#include "render/Shader.h"
#include "render/VulkanImage.h"

#include "glm/glm.hpp"

#include <vector>
#include <memory>

class Material;
class HillairePass : public AtmospherePass
{
public:
	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;

	void SetTransmittanceLUT(const VulkanImage& lut, const VulkanSampler& sampler) { transmittanceLUT = &lut; transmittanceLUTSampler = &sampler; }
protected:
	auto CreateShader(VkDevice device, VkDescriptorSetLayout cameraSetLayout) -> Shader override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;
private:
	const VulkanImage* transmittanceLUT = nullptr;
	const VulkanSampler* transmittanceLUTSampler = nullptr;
};