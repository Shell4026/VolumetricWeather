#pragma once
#include "nlohmann/json.hpp"

#include <type_traits>
#include <vector>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <string>
using Json = nlohmann::json;

template<typename T>
concept PresetType = requires(const T& obj, T& obj2, const Json& json)
{
	{ obj.Serialize() } -> std::same_as<Json>;
	obj2.Deserialize(json);
};

template<PresetType T>
class PresetManager
{
public:
	struct PresetInfo
	{
		std::string name;
		T preset;
	};
public:
	void AddPreset(std::string name, const T& preset);
	auto GetPreset(const std::string& name) const -> const T*;
	void DeletePreset(const std::string& name);

	void ExportPresets(const std::filesystem::path& path) const;
	auto LoadPresets(const std::filesystem::path& path) -> bool;
	auto GetPresets() const -> const std::vector<PresetInfo>& { return presets; }
private:
	std::vector<PresetInfo> presets;
};


template<PresetType T>
inline void PresetManager<T>::AddPreset(std::string name, const T& preset)
{
	if (GetPreset(name) == nullptr)
		presets.push_back(PresetInfo{ std::move(name), preset });
}

template<PresetType T>
inline auto PresetManager<T>::GetPreset(const std::string& name) const -> const T*
{
	for (int i = 0; i < static_cast<int>(presets.size()); ++i)
	{
		if (presets[i].name == name)
			return &presets[i].preset;
	}
	return nullptr;
}

template<PresetType T>
inline void PresetManager<T>::DeletePreset(const std::string& name)
{
	auto it = std::remove_if(presets.begin(), presets.end(), [&](const PresetInfo& preset)
		{
			return preset.name == name;
		}
	);
	presets.erase(it, presets.end());
}

template<PresetType T>
inline void PresetManager<T>::ExportPresets(const std::filesystem::path& path) const
{
	std::ofstream file{ path, std::ios_base::out };

	Json json;
	for (const PresetInfo& preset : presets)
		json[preset.name] = preset.preset.Serialize();
	file << json;
	file.close();
}

template<PresetType T>
inline auto PresetManager<T>::LoadPresets(const std::filesystem::path& path) -> bool
{
	std::ifstream file{ path };
	if (!file.is_open())
		return false;

	Json json;
	json << file;
	file.close();

	presets.clear();
	for (auto it = json.begin(); it != json.end(); ++it)
	{
		PresetInfo preset{};
		preset.name = it.key();
		preset.preset.Deserialize(it.value());
		presets.push_back(std::move(preset));
	}
	return true;
}