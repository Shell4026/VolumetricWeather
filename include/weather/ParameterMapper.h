#pragma once
#include "weather/Setting.h"

#include "pass/CloudPass.h"

#include <glm/vec3.hpp>
class ParameterMapper
{
public:
	static auto ConvertRenderSetting(const ArtistSetting& artistSetting) -> WeatherSetting;
	static auto ConvertCloudSetting(const ArtistSetting& artistSetting) -> CloudPass::Setting;
private:
	static auto GetSunDirection(const ArtistSetting& artistSetting) -> glm::vec3;
};
