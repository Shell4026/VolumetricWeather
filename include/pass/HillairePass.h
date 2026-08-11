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
class HillairePass : public AtmosphereBasePass
{
public:
	struct alignas(16) Setting
	{
		glm::vec4 sun;
		float radius = 6'460'000.f;
		uint32_t modeFlags = 0b0111;
	};
public:
	HillairePass(const LowDepthPass& lowDepthPass, const LUTPass& lutPass, const CloudTRPass& cloudTRPass);
	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;
	void Record(const VulkanContext& ctx, const FrameContext& frame) override;
	void UpdateMaterial();

	void SetSetting(const Setting& setting);
	auto GetSetting() const -> const Setting& { return setting; }
protected:
	auto CreateShader(VkDevice device, VkDescriptorSetLayout cameraSetLayout) -> Shader override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool) override;
private:
	const LowDepthPass& lowDepthPass;
	const LUTPass& lutPass;
	const CloudTRPass& cloudTRPass;

	Setting setting;
};