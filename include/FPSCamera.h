#pragma once
#include "Camera.h"

#include "glm/gtc/quaternion.hpp"
class FPSCamera : public Camera
{
public:
	FPSCamera();

	void SetPos(const glm::vec3& pos) override;
	void SetTo(const glm::vec3& to) override;

	void SetYaw(float degree);
	void AddYaw(float degree);
	void SetPitch(float degree);
	void AddPitch(float degree);
	void SetQuat(const glm::quat quat);
	auto GetQuat() const -> const glm::quat& { return quat; }
private:
	void ApplyQuat();
private:
	float yaw = 0.f;
	float pitch = 0.f;

	glm::quat quat;
	glm::vec3 forward{ 0.f, 0.f, -1.f };
};