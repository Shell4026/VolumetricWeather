#pragma once
#include "Mesh.h"

#include "glm/glm.hpp"

#include <filesystem>
#include <string>
#include <optional>
#include <memory>

class VulkanContext;
class GLBLoader
{
public:
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec2 uv;
		glm::vec3 normal;
	};
	struct Node
	{
		std::string name;
		glm::mat4 modelMatrix{ 1.f };
		std::unique_ptr<Mesh<Vertex>> meshPtr = nullptr;
		std::vector<int> childrenIdxs;
	};
public:
	static auto LoadGLB(const VulkanContext& ctx, const std::filesystem::path& path) -> std::vector<Node>;
};