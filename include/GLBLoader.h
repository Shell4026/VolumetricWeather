#pragma once
#include "render/Mesh.h"
#include "render/VulkanContext.h"
#include "render/VulkanImage.h"

#include "glm/glm.hpp"

#include <filesystem>
#include <string>
#include <optional>
#include <memory>
#include <vector>
struct GLBVertex
{
	glm::vec3 pos;
	glm::vec2 uv;
	glm::vec3 normal;

	static auto GetVertexInputAttributeDescription() -> const std::vector<VkVertexInputAttributeDescription>&
	{
		static std::vector<VkVertexInputAttributeDescription> attribs;
		if (attribs.empty())
		{
			VkVertexInputAttributeDescription& vertexInputAttrib0 = attribs.emplace_back();
			vertexInputAttrib0.binding = 0;
			vertexInputAttrib0.location = 0;
			vertexInputAttrib0.format = VkFormat::VK_FORMAT_R32G32B32_SFLOAT;
			vertexInputAttrib0.offset = offsetof(GLBVertex, pos);
			VkVertexInputAttributeDescription& vertexInputAttrib1 = attribs.emplace_back();
			vertexInputAttrib1.binding = 0;
			vertexInputAttrib1.location = 1;
			vertexInputAttrib1.format = VkFormat::VK_FORMAT_R32G32_SFLOAT;
			vertexInputAttrib1.offset = offsetof(GLBVertex, uv);
			VkVertexInputAttributeDescription& vertexInputAttrib2 = attribs.emplace_back();
			vertexInputAttrib2.binding = 0;
			vertexInputAttrib2.location = 2;
			vertexInputAttrib2.format = VkFormat::VK_FORMAT_R32G32B32_SFLOAT;
			vertexInputAttrib2.offset = offsetof(GLBVertex, normal);
		}
		return attribs;
	}
	static auto GetVertexInputBindingDescription() -> VkVertexInputBindingDescription
	{
		static VkVertexInputBindingDescription vertexInputBinding{};
		vertexInputBinding.binding = 0;
		vertexInputBinding.stride = sizeof(GLBVertex);
		vertexInputBinding.inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;
		return vertexInputBinding;
	}
};

class GLBLoader
{
public:
	struct Node
	{
		std::string name;
		glm::mat4 modelMatrix{ 1.f };
		std::unique_ptr<Mesh<GLBVertex>> meshPtr = nullptr;
		std::vector<int> childrenIdxs;
		int textureIdx = -1;
	};
	struct Model
	{
		std::vector<VulkanImage> textures;
		std::vector<Node> nodes;
	};
public:
	static auto LoadGLB(const VulkanContext& ctx, const std::filesystem::path& path) -> Model;
};