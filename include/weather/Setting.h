#pragma once
#include "Bezier.hpp"
#include "glm/glm.hpp"

struct ArtistSetting
{
	float planetRotationAxis = 23.4f; // 지구 자전축
	float atmosphereThickness = 1.f;

	float latitude = 37.f; // 대한민국 위도
	float hour = 12.f;
	float month = 6.f;
	
	float cloudAmount = 0.35f;
	float cloudSizeKM = 100.f;
	float cloudBrightness = 1.0f;
	Bezier cloudBrightnessBezier;

	float detail = 0.5f;
	float density = 0.4f;
	float verticalDevelopment = 0.4f;

	
	float softness = 0.5f;

	float windDirectionDegrees = 0.0f;
	float windSpeedKmh = 10.0f;
};

struct WeatherSetting
{
	struct alignas(16) Atmosphere
	{
		float groundRadius = 6'360'000.f;
		float atmosphereRadius = 6'460'000.f;
		float mieCoefficient = 1.f;
		float mieG = 0.8;

		glm::vec3 mieColor{ 3.996f,  3.996f, 3.996f };
		char padding0;

		glm::vec3 rayleighColor{ 5.802f, 13.558f, 33.1f };
		char padding1;
	} atmosphere;
	struct alignas(16) Lighting
	{
		glm::vec4 sun{ 1.f, -1.f, -1.f, 15.f };
		glm::mat4 sunViewProj{ 1.f };
	} lighting;
};

enum WeatherDirtyFlag
{
	Atmosphere = 1,
	Lighting = 2,
	Cloud = 4
};
using WeatherDirtyFlags = uint32_t;