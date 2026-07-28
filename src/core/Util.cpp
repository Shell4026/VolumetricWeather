#include "core/Util.h"
#include "core/Logger.h"

#include <fstream>
namespace util
{
	void SaveBinary(const std::vector<uint8_t>& binary, const std::filesystem::path& path)
	{
		std::ofstream file{ path, std::ios::binary };
		if (!file)
		{
			SH_ERROR_FORMAT("Failed to save: {}", path.string());
			return;
		}
		file.write(reinterpret_cast<const char*>(binary.data()), binary.size());
		file.close();
	}
	auto LoadBinary(const std::filesystem::path& path) -> std::vector<uint8_t>
	{
		std::ifstream file{ path, std::ios::ate | std::ios::binary };
		if (!file.is_open())
		{
			SH_ERROR_FORMAT("Failed to read: {}", path.string());
			return {};
		}
		const std::size_t fileSize = (size_t)file.tellg();
		std::vector<uint8_t> binary(fileSize);
		file.seekg(0);
		file.read(reinterpret_cast<char*>(binary.data()), fileSize);
		file.close();

		return binary;
	}
}