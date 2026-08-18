#define GLM_ENABLE_EXPERIMENTAL
#include "weather/ParameterMapper.h"

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/vec_swizzle.hpp"
#include <cmath>

auto ParameterMapper::ConvertRenderSetting(const ArtistSetting& artistSetting) -> WeatherSetting
{
	WeatherSetting setting{};
	setting.atmosphere.atmosphereRadius = setting.atmosphere.groundRadius + 100'000.f * artistSetting.atmosphereThickness;
	setting.atmosphere.rayleighColor *= glm::clamp(artistSetting.rayleighScatteringStrength, 0.f, 4.f);
	setting.atmosphere.mieCoefficient = artistSetting.mieScatteringStrength;
	setting.atmosphere.mieG = glm::clamp(artistSetting.mieAnisotropy, 0.f, 0.99f);
	setting.atmosphere.mieColor = glm::vec3{ 3.996f,  3.996f, 3.996f } * glm::vec3{ artistSetting.mieColor };

	setting.lighting.sun = glm::vec4{ GetSunDirection(artistSetting), 15.f * artistSetting.sunIntensity };

	return setting;
}

auto ParameterMapper::ConvertCloudSetting(const ArtistSetting& artistSetting) -> CloudPass::Setting
{
	CloudPass::Setting result{};
	const float amount = glm::clamp(artistSetting.cloudAmount, 0.f, 1.f);
	const float sizeKM = glm::clamp(artistSetting.cloudSizeKM, 10.f, 100.f);
	const float detail = glm::clamp(artistSetting.cloudDetail, 0.f, 1.f);
	const float density = glm::clamp(artistSetting.cloudDensity, 0.f, 1.f);

	result.coverage = glm::mix(0.f, 0.2f, amount);
	result.tiling = sizeKM * 1'000.f;
	result.tiling2 = glm::mix(10'000.f, 500.f, detail);
	result.extinctionCoefficient = glm::mix(10.f, 500.f, density * density);
	result.anvilBias = (artistSetting.cloudVerticalAmount + 1.f) / 2.f; // -1~1 -> 0~1
	result.brightnessStrength = glm::clamp(artistSetting.cloudBrightness, 0.f, 4.f);
	result.brightnessCurve = artistSetting.cloudBrightnessBezier;
	result.powderStrength = glm::clamp(artistSetting.cloudSoftness, 0.f, 1.f);

	const float directionRadians = glm::radians(artistSetting.windDirectionDegrees);
	result.windVelKmh = glm::vec2{ std::cos(directionRadians), std::sin(directionRadians) } * artistSetting.windSpeedKmh;

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
