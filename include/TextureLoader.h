#pragma once
#include "render/VulkanImage.h"

#include <filesystem>
#include <optional>
class VulkanContext;
class TextureLoader
{
public:
	static auto Load(const VulkanContext& ctx, const std::filesystem::path& filePath) -> std::optional<VulkanImage>;
};