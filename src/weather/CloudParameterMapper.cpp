#include "weather/CloudParameterMapper.h"

#include "glm/glm.hpp"

#include <cmath>

auto CloudParameterMapper::ConvertRenderSetting(const ArtistCloudSetting& artistSetting, const CloudPass::Setting& baseSetting) -> CloudPass::Setting
{
	CloudPass::Setting result = baseSetting;

	const float amount = glm::clamp(artistSetting.amount, 0.0f, 1.0f);
	const float size = glm::clamp(artistSetting.size, 0.0f, 1.0f);
	const float detail = glm::clamp(artistSetting.detail, 0.0f, 1.0f);
	const float density = glm::clamp(artistSetting.density, 0.0f, 1.0f);
	const float verticalDevelopment = glm::clamp(artistSetting.verticalDevelopment, 0.0f, 1.0f);
	const float darkness = glm::clamp(artistSetting.darkness, 0.0f, 1.0f);
	const float softness = glm::clamp(artistSetting.softness, 0.0f, 1.0f);

	result.coverage = glm::mix(0.8f, 0.02f, glm::smoothstep(0.0f, 1.0f, amount));
	result.tiling = glm::mix(10'000.0f, 150'000.0f, size);
	result.tiling2 = glm::mix(10'000.0f, 500.0f, detail);
	result.extinctionCoefficient = glm::mix(10.0f, 500.0f, density * density);
	result.anvilBias = glm::smoothstep(0.55f, 1.0f, verticalDevelopment);
	result.darkStrength = glm::mix(0.2f, 4.0f, darkness);
	result.darkHeight = glm::mix(0.95f, 0.45f, darkness);
	result.powderStrength = softness;

	const float directionRadians = glm::radians(artistSetting.windDirectionDegrees);
	const float speedKmh = glm::max(artistSetting.windSpeedKmh, 0.0f);
	result.windVelKmh = glm::vec2{ std::cos(directionRadians), std::sin(directionRadians) } * speedKmh;

	return result;
}