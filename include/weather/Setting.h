#pragma once
#include "glm/glm.hpp"

struct ArtistSetting
{
	float planetRotationAxis = 23.4; // 지구 자전축
	float latitude = 37.f; // 대한민국 위도
	float hour = 12.f;
	float month = 6.f;

	float amount = 0.35f;
	float size = 0.5f;
	float detail = 0.5f;
	float density = 0.4f;
	float verticalDevelopment = 0.4f;

	float darkness = 0.2f;
	float softness = 0.5f;

	float windDirectionDegrees = 0.0f;
	float windSpeedKmh = 10.0f;
};

struct WeatherSetting
{
	glm::vec4 sun{ 1.f, -1.f, -1.f, 15.f };
};