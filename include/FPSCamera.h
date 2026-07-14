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
private:
	float yaw = 270.f;
	float pitch = 0.f;

	glm::quat quat;
	glm::vec3 forward{ 0.f, 0.f, -1.f };
};