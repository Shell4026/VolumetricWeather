#pragma once

#include "weather/setting.h"

inline void to_json(nlohmann::json& json, const Bezier& value)
{
	json =
	{
		value.a.x,
		value.a.y,
		value.b.x,
		value.b.y,
		value.c.x,
		value.c.y,
		value.d.x,
		value.d.y,
	};
}

inline void from_json(const nlohmann::json& json, Bezier& value)
{
	value.a.x = json.at(0).get<float>();
	value.a.y = json.at(1).get<float>();
	value.b.x = json.at(2).get<float>();
	value.b.y = json.at(3).get<float>();
	value.c.x = json.at(4).get<float>();
	value.c.y = json.at(5).get<float>();
	value.d.x = json.at(6).get<float>();
	value.d.y = json.at(7).get<float>();
}

auto ArtistSetting::Serialize() const -> Json
{
	Json json;

	json["planetRotationAxis"] = planetRotationAxis;
	json["atmosphereThickness"] = atmosphereThickness;
	json["rayleighScatteringStrength"] = rayleighScatteringStrength;
	json["mieScatteringStrength"] = mieScatteringStrength;
	json["mieAnisotropy"] = mieAnisotropy;
	json["mieColor"] = {
		mieColor.x,
		mieColor.y,
		mieColor.z,
		mieColor.w
	};

	json["latitude"] = latitude;
	json["hour"] = hour;
	json["month"] = month;
	json["sunIntensity"] = sunIntensity;

	json["cloudAmount"] = cloudAmount;
	json["cloudSizeKM"] = cloudSizeKM;
	json["cloudBrightness"] = cloudBrightness;
	json["cloudBrightnessBezier"] = cloudBrightnessBezier;
	json["cloudDetail"] = cloudDetail;
	json["cloudDensity"] = cloudDensity;
	json["cloudVerticalAmount"] = cloudVerticalAmount;
	json["cloudSoftness"] = cloudSoftness;
	json["cloudColor"] = {
		cloudColor.x,
		cloudColor.y,
		cloudColor.z,
		cloudColor.w
	};

	json["windDirectionDegrees"] = windDirectionDegrees;
	json["windSpeedKmh"] = windSpeedKmh;

	return json;
}
void ArtistSetting::Deserialize(const Json& json)
{
	planetRotationAxis = json.value("planetRotationAxis", planetRotationAxis);
	atmosphereThickness = json.value("atmosphereThickness", atmosphereThickness);
	rayleighScatteringStrength = json.value("rayleighScatteringStrength", rayleighScatteringStrength);
	mieScatteringStrength = json.value("mieScatteringStrength", mieScatteringStrength);
	mieAnisotropy = json.value("mieAnisotropy", mieAnisotropy);
	mieColor = json.value("mieColor", mieColor);

	latitude = json.value("latitude", latitude);
	hour = json.value("hour", hour);
	month = json.value("month", month);
	sunIntensity = json.value("sunIntensity", sunIntensity);

	cloudAmount = json.value("cloudAmount", cloudAmount);
	cloudSizeKM = json.value("cloudSizeKM", cloudSizeKM);
	cloudBrightness = json.value("cloudBrightness", cloudBrightness);

	cloudBrightnessBezier = json.value("cloudBrightnessBezier", cloudBrightnessBezier);

	cloudDetail = json.value("cloudDetail", cloudDetail);
	cloudDensity = json.value("cloudDensity", cloudDensity);
	cloudVerticalAmount = json.value("cloudVerticalAmount", cloudVerticalAmount);
	cloudSoftness = json.value("cloudSoftness", cloudSoftness);
	cloudColor = json.value("cloudColor", cloudColor);

	windDirectionDegrees = json.value("windDirectionDegrees", windDirectionDegrees);
	windSpeedKmh = json.value("windSpeedKmh", windSpeedKmh);
}