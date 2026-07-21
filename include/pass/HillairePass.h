#pragma once
#include "AtmospherePass.h"
#include "render/Shader.h"
#include "render/VulkanImage.h"

#include "glm/glm.hpp"

#include <vector>
#include <memory>

class Material;
class LUTPass;
class HillairePass : public AtmospherePass
{
public:
	HillairePass(const LUTPass& lutPass);
	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;
protected:
	auto CreateShader(VkDevice device, VkDescriptorSetLayout cameraSetLayout) -> Shader override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;
private:
	const LUTPass& lutPass;
};