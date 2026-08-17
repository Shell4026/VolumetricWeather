#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>
namespace util
{
	void SaveBinary(const std::vector<uint8_t>& binary, const std::filesystem::path& path);
	auto LoadBinary(const std::filesystem::path& path) -> std::vector<uint8_t>;
	inline auto CombineHash(size_t v1, size_t v2) -> size_t
	{
		return v1 ^= v2 + 0x9e3779b9 + (v1 << 6) + (v1 >> 2);
	}
	inline auto U8Str(const char8_t* u8) -> const char*
	{
		return reinterpret_cast<const char*>(u8);
	}
}