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

	void SetTransmittanceLUTSampler(const VulkanSampler& sampler) { transmittanceLUTSampler = &sampler; }
	void SetTransmittanceLUT(const VulkanImage& lut) { transmittanceLUT = &lut; }
	void SetSkyViewLUTSampler(const VulkanSampler& sampler) { skyViewLUTSampler = &sampler; }
	void SetSkyViewLUT(const VulkanImage& lut) { skyViewLUT = &lut; }
protected:
	auto CreateShader(VkDevice device, VkDescriptorSetLayout cameraSetLayout) -> Shader override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;
private:
	const VulkanImage* transmittanceLUT = nullptr;
	const VulkanImage* skyViewLUT = nullptr;
	const VulkanSampler* transmittanceLUTSampler = nullptr;
	const VulkanSampler* skyViewLUTSampler = nullptr;
};