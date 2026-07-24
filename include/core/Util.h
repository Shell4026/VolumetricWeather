#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>
namespace util
{
	auto LoadBinary(const std::filesystem::path& path) -> std::vector<uint8_t>;
}