#pragma once
#include "Setting.h"

class IWeatherPass
{
public:
	virtual ~IWeatherPass() = default;

	virtual void SetSetting(const WeatherSetting::Atmosphere& atmosphereSetting) = 0;
	virtual void SetSetting(const WeatherSetting::Lighting& lightingSetting) = 0;
};