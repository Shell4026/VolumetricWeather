#pragma once
#include "weather/Setting.h"

#include <glm/vec3.hpp>
class ParameterMapper
{
public:
	static auto ConvertRenderSetting(const ArtistSetting& artistSetting) -> WeatherSetting;
private:
	static auto GetSunDirection(const ArtistSetting& artistSetting) -> glm::vec3;
};
