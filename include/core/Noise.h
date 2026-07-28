#pragma once

#include "glm/glm.hpp"

#include <vector>
class Noise
{
public:
	using Texel = std::vector<uint8_t>;
public:
	static auto GeneratePerlinNoiseTexture(uint32_t width, uint32_t height, uint32_t depth, float frequency = 1, int octaveCount = 4) -> Texel;
	static auto GenerateWorleyNoiseTexture(uint32_t width, uint32_t height, uint32_t depth) -> Texel;
	static auto GeneratePerlinWorleyNoiseTexture(uint32_t width, uint32_t height, uint32_t depth, float frequency) -> Texel;

	static auto Remap(float value, float originalMin, float originalMax, float newMin, float newMax) -> float;
private:
	static auto PerlinNoise(const glm::vec3& pIn, float frequency, int octaveCount) -> float;
	static auto WorleyNoise(const glm::vec3& pIn, float frequency) -> float;
	static auto WorleyNoiseFBM(const glm::vec3& pIn) -> float;
	static auto Hash(float n) -> float;
	static auto NoiseF(const glm::vec3& x) -> float;
};