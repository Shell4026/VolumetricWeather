#pragma once

#include "weather/ArtistCloudSetting.h"

#include "pass/CloudPass.h"

class CloudParameterMapper
{
public:
	static auto ConvertRenderSetting(const ArtistCloudSetting& artistSetting, const CloudPass::Setting& baseSetting) -> CloudPass::Setting;
};
