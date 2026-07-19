#include "AtmosphereRMSEMeasurement.h"

#include "pass/BlitPass.h"
#include "render/VulkanImage.h"

#include <chrono>
#include <cmath>
#include <stdexcept>
#include <utility>

void AtmosphereRMSEMeasurement::Start(bool restoreHillaire)
{
	if (IsRunning())
		return;

	this->restoreHillaire = restoreHillaire;
	result.reset();
	error.clear();
	originalPixels.clear();
	stage = Stage::CaptureOriginal;
}

auto AtmosphereRMSEMeasurement::Update(
	BlitPass& blitPass,
	const VulkanImage& originalImage,
	const VulkanImage& hillaireImage) -> std::optional<bool>
{
	try
	{
		switch (stage)
		{
		case Stage::Idle:
			return std::nullopt;
		case Stage::CaptureOriginal:
		{
			const VkExtent3D extent = originalImage.GetInfo().extent;
			if (extent.width == 0 || extent.height == 0)
				throw std::runtime_error{ "The original atmosphere image has an invalid extent" };

			readback = blitPass.RequestBlit(originalImage, 0, 0, extent.width, extent.height);
			stage = Stage::WaitOriginal;
			return false;
		}
		case Stage::WaitOriginal:
			if (readback.wait_for(std::chrono::milliseconds{ 0 }) != std::future_status::ready)
				return std::nullopt;

			originalPixels = readback.get();
			if (originalPixels.empty())
				throw std::runtime_error{ "The original atmosphere readback returned no pixels" };
			{
				const VkExtent3D extent = hillaireImage.GetInfo().extent;
				if (extent.width == 0 || extent.height == 0)
					throw std::runtime_error{ "The Hillaire atmosphere image has an invalid extent" };
				readback = blitPass.RequestBlit(hillaireImage, 0, 0, extent.width, extent.height);
			}
			stage = Stage::WaitHillaire;
			return true;
		case Stage::WaitHillaire:
			if (readback.wait_for(std::chrono::milliseconds{ 0 }) != std::future_status::ready)
				return std::nullopt;

			result = CalculateRMSE(originalPixels, readback.get());
			originalPixels.clear();
			stage = Stage::Idle;
			return restoreHillaire;
		}
	}
	catch (const std::exception& e)
	{
		Fail(e.what());
		return restoreHillaire;
	}

	return std::nullopt;
}

auto AtmosphereRMSEMeasurement::GetStatus() const -> const char*
{
	switch (stage)
	{
	case Stage::Idle:
		return "Idle";
	case Stage::CaptureOriginal:
		return "Preparing original atmosphere capture";
	case Stage::WaitOriginal:
		return "Reading original atmosphere pixels";
	case Stage::WaitHillaire:
		return "Reading Hillaire atmosphere pixels";
	}
	return "Unknown";
}

auto AtmosphereRMSEMeasurement::CalculateRMSE(
	const std::vector<glm::vec4>& original,
	const std::vector<glm::vec4>& hillaire) -> Result
{
	if (original.size() != hillaire.size())
		throw std::runtime_error{ "Atmosphere output image sizes do not match" };
	if (original.empty())
		throw std::runtime_error{ "There are no atmosphere pixels to compare" };

	glm::dvec3 squaredError{ 0.0 };
	for (std::size_t i = 0; i < original.size(); ++i)
	{
		const glm::dvec3 difference = glm::dvec3{ original[i] } - glm::dvec3{ hillaire[i] };
		squaredError += difference * difference;
	}

	Result result;
	result.pixelCount = original.size();
	const glm::dvec3 channelMSE = squaredError / static_cast<double>(original.size());
	result.channelRMSE = {
		std::sqrt(channelMSE.x),
		std::sqrt(channelMSE.y),
		std::sqrt(channelMSE.z)
	};
	result.rmse = std::sqrt((squaredError.x + squaredError.y + squaredError.z) /
		(static_cast<double>(original.size()) * 3.0));
	return result;
}

void AtmosphereRMSEMeasurement::Fail(std::string message)
{
	error = std::move(message);
	result.reset();
	originalPixels.clear();
	stage = Stage::Idle;
}
