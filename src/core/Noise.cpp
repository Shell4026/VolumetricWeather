#include "core/Noise.h"

#include "glm/gtc/noise.hpp"

auto Noise::GeneratePerlinNoiseTexture(uint32_t width, uint32_t height, uint32_t depth, float frequency, int octaveCount) -> Texel
{
	Texel result(depth * height * width, 0);

	for (int z = 0; z < depth; ++z)
	{
		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				const glm::vec3 pIn = glm::vec3(x, y, z) / glm::vec3(width, height, depth);
				const uint32_t wh = width * height;
				result[x + width * y + wh * z] = static_cast<uint8_t>(std::floor(PerlinNoise(pIn, frequency, octaveCount) * 255.f));
			}
		}
	}
	return result;
}

auto Noise::GenerateWorleyNoiseTexture(uint32_t width, uint32_t height, uint32_t depth, float f0, float f1, float f2) -> Texel
{
	Texel result(depth * height * width, 0);
	for (int z = 0; z < depth; ++z)
	{
		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				const glm::vec3 pIn = glm::vec3(x, y, z) / glm::vec3(width, height, depth);
				const uint32_t wh = width * height;
				result[x + width * y + wh * z] = static_cast<uint8_t>(std::floor(WorleyNoiseFBM(pIn, f0, f1, f2) * 255.f));
			}
		}
	}
	return result;
}

auto Noise::GeneratePerlinWorleyNoiseTexture(uint32_t width, uint32_t height, uint32_t depth, float frequency) -> Texel
{
	Texel result(depth * height * width, 0);

	for (int z = 0; z < depth; ++z)
	{
		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				const glm::vec3 pIn = glm::vec3(x, y, z) / glm::vec3(width, height, depth);
				const uint32_t wh = width * height;
				const float perlin = PerlinNoise(pIn, frequency, 3);
				const float worley = 1.0 - WorleyNoiseFBM(pIn, 2.f, 8.f, 14.f);
				const float perlinWorley = Remap(perlin, 0.0f, 1.0f, worley, 1.0f);
				result[x + width * y + wh * z] = static_cast<uint8_t>(std::floor(perlinWorley * 255.f));
			}
		}
	}
	return result;
}

auto Noise::Remap(float value, float originalMin, float originalMax, float newMin, float newMax) -> float
{
	return newMin + ((value - originalMin) / (originalMax - originalMin)) * (newMax - newMin);
}

auto Noise::PerlinNoise(const glm::vec3& pIn, float frequency, int octaveCount) -> float
{
	const float octaveFrenquencyFactor = 2;			// noise frequency factor between octave, forced to 2

	// Compute the sum for each octave
	float sum = 0.0f;
	float weightSum = 0.0f;
	float weight = 0.5f;
	for (int oct = 0; oct < octaveCount; oct++)
	{
		// Perlin vec3 is bugged in GLM on the Z axis :(, black stripes are visible
		// So instead we use 4d Perlin and only use xyz...
		//glm::vec3 p(x * freq, y * freq, z * freq);
		//float val = glm::perlin(p, glm::vec3(freq)) *0.5 + 0.5;

		glm::vec4 p = glm::vec4(pIn.x, pIn.y, pIn.z, 0.0f) * glm::vec4(frequency);
		float val = glm::perlin(p, glm::vec4(frequency));

		sum += val * weight;
		weightSum += weight;

		weight *= weight;
		frequency *= octaveFrenquencyFactor;
	}

	float noise = (sum / weightSum) * 0.5f + 0.5f;
	noise = std::fminf(noise, 1.0f);
	noise = std::fmaxf(noise, 0.0f);
	return noise;
}

auto Noise::WorleyNoise(const glm::vec3& pIn, float frequency) -> float
{
	const glm::vec3 pCell = pIn * frequency;
	float d = 1.0e10;
	for (int xo = -1; xo <= 1; xo++)
	{
		for (int yo = -1; yo <= 1; yo++)
		{
			for (int zo = -1; zo <= 1; zo++)
			{
				glm::vec3 tp = glm::floor(pCell) + glm::vec3(xo, yo, zo);

				tp = pCell - tp - NoiseF(glm::mod(tp, frequency / 1));

				d = glm::min(d, dot(tp, tp));
			}
		}
	}
	d = std::fminf(d, 1.0f);
	d = std::fmaxf(d, 0.0f);
	return d;
}

auto Noise::WorleyNoiseFBM(const glm::vec3& pIn, float f0, float f1, float f2) -> float
{
	const float cellCount = 4;
	return
		WorleyNoise(pIn, cellCount * f0) * 0.625f +
		WorleyNoise(pIn, cellCount * f1) * 0.25f +
		WorleyNoise(pIn, cellCount * f2) * 0.125f;
}

auto Noise::Hash(float n) -> float
{
	return glm::fract(sin(n + 1.951f) * 43758.5453f);
}

auto Noise::NoiseF(const glm::vec3& x) -> float
{
	glm::vec3 p = glm::floor(x);
	glm::vec3 f = glm::fract(x);

	f = f * f * (glm::vec3(3.0f) - glm::vec3(2.0f) * f);
	float n = p.x + p.y * 57.0f + 113.0f * p.z;
	return glm::mix(
		glm::mix(
			glm::mix(Hash(n + 0.0f), Hash(n + 1.0f), f.x),
			glm::mix(Hash(n + 57.0f), Hash(n + 58.0f), f.x),
			f.y),
		glm::mix(
			glm::mix(Hash(n + 113.0f), Hash(n + 114.0f), f.x),
			glm::mix(Hash(n + 170.0f), Hash(n + 171.0f), f.x),
			f.y),
		f.z);
}
