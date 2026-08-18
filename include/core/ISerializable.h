#pragma once

#include <nlohmann/json.hpp>
#include <glm/vec4.hpp>

using Json = nlohmann::json;
class ISerializable
{
public:
	virtual ~ISerializable() = default;
	virtual auto Serialize() const -> Json = 0;
	virtual void Deserialize(const Json& json) = 0;
};

namespace glm
{
	inline void to_json(nlohmann::json& json, const glm::vec4& value)
	{
		json =
		{
			value.x,
			value.y,
			value.z,
			value.w
		};
	}

	inline void from_json(const nlohmann::json& json, glm::vec4& value)
	{
		value.x = json.at(0).get<float>();
		value.y = json.at(1).get<float>();
		value.z = json.at(2).get<float>();
		value.w = json.at(3).get<float>();
	}
}