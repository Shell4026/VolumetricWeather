#pragma once
#include "AtmospherePass.h"
#include "render/Shader.h"
#include "render/VulkanImage.h"

#include "glm/glm.hpp"

#include <vector>
#include <memory>

class Material;
class LowDepthPass;
class LUTPass;
class CloudTRPass;
class HillairePass : public AtmospherePass
{
public:
	HillairePass(const LowDepthPass& lowDepthPass, const LUTPass& lutPass, const CloudTRPass& cloudTRPass);
	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;
	void Record(const VulkanContext& ctx, const FrameContext& frame) override;
	void UpdateMaterial();
protected:
	auto CreateShader(VkDevice device, VkDescriptorSetLayout cameraSetLayout) -> Shader override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;
private:
	const LowDepthPass& lowDepthPass;
	const LUTPass& lutPass;
	const CloudTRPass& cloudTRPass;
};