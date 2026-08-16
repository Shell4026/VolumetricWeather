#pragma once
#include "glm/vec2.hpp"

struct alignas(16) Bezier
{
    glm::vec2 a{ 0.f, 0.f };
    glm::vec2 b{ 0.2f, 0.5f };
    glm::vec2 c{ 0.8f, 0.5f };
    glm::vec2 d{ 1.f, 1.f };

	auto GetSample(float t) const -> glm::vec2
	{
        t = std::clamp(t, 0.0f, 1.0f);

        const glm::vec2 center = (b + c) * 0.5f;

        if (t < 0.5f)
        {
            const float s = t * 2.0f;
            const float ss = s * s;
            const float u = 1.0f - s;
            const float uu = u * u;

            return uu * a + 2.0f * u * s * b + ss * center;
        }

        const float s = (t - 0.5f) * 2.0f;
        const float ss = s * s;
        const float u = 1.0f - s;
        const float uu = u * u;

        return uu * center + 2.0f * u * s * c + ss * d;
	}
};