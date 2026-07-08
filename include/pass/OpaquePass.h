#pragma once
#include "APass.h"
#include "Mesh.h"
#include "GLBLoader.h"
#include "Shader.h"

#include "glm/glm.hpp"

#include <memory>
#include <vector>
class VulkanBuffer;
class VulkanImage;
class OpaquePass : public APass
{
public:
	void Clear(const VulkanContext& ctx, VkDescriptorPool descPool) override;

	void Record(const VulkanContext& ctx, const FrameContext& frame) override;

	void SetUsages(const VulkanContext& ctx, const FrameContext& frame) override;
	void PushDrawMesh(const AMeshBase& mesh);

	auto GetShader() const -> const Shader& { return opaqueShader; }
protected:
	void PrepareResource(const VulkanContext& ctx) override;
	void SetupDescriptors(const VulkanContext& ctx, VkDescriptorPool descPool, VkDescriptorSetLayout cameraSetLayout) override;
	void BuildPipeline(const VulkanContext& ctx) override;
private:
	Shader opaqueShader;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkDescriptorSet descSet1 = VK_NULL_HANDLE;

	glm::vec4 color{ 1.f, 1.f, 1.f, 1.f };
	std::unique_ptr<VulkanBuffer> buffer;
	std::unique_ptr<VulkanImage> outputImage;

	std::vector<const AMeshBase*> meshes;
};