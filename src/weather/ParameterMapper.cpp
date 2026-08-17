#include "weather/ParameterMapper.h"

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include <cmath>

auto ParameterMapper::ConvertRenderSetting(const ArtistSetting& artistSetting) -> WeatherSetting
{
	WeatherSetting setting{};
	setting.lighting.sun = glm::vec4{ GetSunDirection(artistSetting), 15.f };

	//const float amount = glm::clamp(artistSetting.amount, 0.0f, 1.0f);
	//const float size = glm::clamp(artistSetting.size, 0.0f, 1.0f);
	//const float detail = glm::clamp(artistSetting.detail, 0.0f, 1.0f);
	//const float density = glm::clamp(artistSetting.density, 0.0f, 1.0f);
	//const float verticalDevelopment = glm::clamp(artistSetting.verticalDevelopment, 0.0f, 1.0f);
	//const float darkness = glm::clamp(artistSetting.darkness, 0.0f, 1.0f);
	//const float softness = glm::clamp(artistSetting.softness, 0.0f, 1.0f);

	//result.coverage = glm::mix(0.8f, 0.02f, glm::smoothstep(0.0f, 1.0f, amount));
	//result.tiling = glm::mix(10'000.0f, 150'000.0f, size);
	//result.tiling2 = glm::mix(10'000.0f, 500.0f, detail);
	//result.extinctionCoefficient = glm::mix(10.0f, 500.0f, density * density);
	//result.anvilBias = glm::smoothstep(0.55f, 1.0f, verticalDevelopment);
	//result.darkStrength = glm::mix(0.2f, 4.0f, darkness);
	//result.darkHeight = glm::mix(0.95f, 0.45f, darkness);
	//result.powderStrength = softness;

	//const float directionRadians = glm::radians(artistSetting.windDirectionDegrees);
	//const float speedKmh = glm::max(artistSetting.windSpeedKmh, 0.0f);
	//result.windVelKmh = glm::vec2{ std::cos(directionRadians), std::sin(directionRadians) } * speedKmh;

	return setting;
}

auto ParameterMapper::ConvertCloudSetting(const ArtistSetting& artistSetting) -> CloudPass::Setting
{
	CloudPass::Setting result{};
	{
		Bezier bezier{};
		bezier.a = { 0.f, 0.f };
		bezier.b = { 0.1f, 0.1f };
		const glm::vec2 center{ 0.5f, 0.1f };
		bezier.c = { 2.f * center - bezier.b };
		bezier.d = { 1.f, 0.2f };
		result.coverage = glm::mix(0.f, 0.2f, artistSetting.cloudAmount);//bezier.GetSample(artistSetting.cloudAmount).y;
	}
	result.brightnessStrength = glm::clamp(artistSetting.cloudBrightness, 0.f, 1.f);
	result.brightnessCurve = artistSetting.cloudBrightnessBezier;

	return result;
}

auto ParameterMapper::GetSunDirection(const ArtistSetting& artistSetting) -> glm::vec3
{
	const float planetDir = glm::radians(artistSetting.planetRotationAxis);
	const float latitude = glm::radians(artistSetting.latitude);
	constexpr float PI = glm::pi<float>();
	const float yearRadians = (artistSetting.month - 1) * (2.f * PI / 12.f);
	const float noonPhase = std::atan2(-glm::sin(yearRadians), glm::cos(planetDir) * glm::cos(yearRadians));
	const float hourRadians = (artistSetting.hour - 12) * (2.f * PI / 24.f) + noonPhase;

	glm::vec3 playerPos;
	playerPos.x = glm::cos(latitude);
	playerPos.y = glm::sin(latitude);
	playerPos.z = 0.f;

	const glm::quat planetRotQ = glm::angleAxis(planetDir, glm::vec3{ 0.f, 0.f, 1.f });
	const glm::vec3 planetUp = glm::normalize(planetRotQ * glm::vec3{ 0.f, 1.f, 0.f });
	playerPos = glm::normalize(planetRotQ * playerPos);

	const glm::quat timeQ = glm::angleAxis(hourRadians, planetUp);
	playerPos = glm::normalize(timeQ * playerPos);

	const glm::vec3 sunDir{ glm::cos(yearRadians), 0.f, glm::sin(yearRadians) };

	const glm::vec3 localUp = glm::normalize(playerPos);
	const glm::vec3 localEast = glm::normalize(glm::cross(planetUp, localUp));
	const glm::vec3 localNorth = glm::normalize(glm::cross(localUp, localEast));

	glm::vec3 localSunDir{ 0.f };
	localSunDir.x = glm::dot(sunDir, localEast);
	localSunDir.y = glm::dot(sunDir, localUp);
	localSunDir.z = glm::dot(sunDir, localNorth);

	localSunDir = -glm::normalize(localSunDir);
	return localSunDir;
}
