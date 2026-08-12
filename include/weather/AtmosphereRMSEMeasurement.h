#pragma once

#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

#include <cstddef>
#include <future>
#include <optional>
#include <string>
#include <vector>

class BlitPass;
class VulkanImage;

class AtmosphereRMSEMeasurement
{
public:
	struct Result
	{
		double rmse = 0.0;
		glm::dvec3 channelRMSE{ 0.0 };
		std::size_t pixelCount = 0;
	};

	void Start(bool restoreHillaire);

	// A returned value requests the atmosphere model to use for this frame.
	// false selects the original implementation and true selects Hillaire.
	auto Update(BlitPass& blitPass, const VulkanImage& originalImage, const VulkanImage& hillaireImage)
		-> std::optional<bool>;

	auto IsRunning() const -> bool { return stage != Stage::Idle; }
	auto GetResult() const -> const std::optional<Result>& { return result; }
	auto GetError() const -> const std::string& { return error; }
	auto GetStatus() const -> const char*;

private:
	enum class Stage
	{
		Idle,
		CaptureOriginal,
		WaitOriginal,
		WaitHillaire
	};

	static auto CalculateRMSE(const std::vector<glm::vec4>& original, const std::vector<glm::vec4>& hillaire)
		-> Result;
	void Fail(std::string message);

	Stage stage = Stage::Idle;
	bool restoreHillaire = false;
	std::future<std::vector<glm::vec4>> readback;
	std::vector<glm::vec4> originalPixels;
	std::optional<Result> result;
	std::string error;
};
