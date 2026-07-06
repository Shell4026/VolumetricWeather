#include "FPSCamera.h"

FPSCamera::FPSCamera()
{
	CalcTo();
}

void FPSCamera::SetYaw(float degree)
{
	yaw = degree;
	yaw = std::fmodf(yaw, 360.f);
	CalcTo();
}

void FPSCamera::AddYaw(float degree)
{
	yaw += degree;
	yaw = std::fmodf(yaw, 360.f);
	CalcTo();
}

void FPSCamera::SetPitch(float degree)
{
	pitch = degree;
	pitch = std::fmodf(pitch, 360.f);
	CalcTo();
}

void FPSCamera::AddPitch(float degree)
{
	pitch += degree;
	pitch = std::fmodf(pitch, 360.f);
	CalcTo();
}

void FPSCamera::CalcTo()
{
	float x = glm::cos(glm::radians(pitch)) * glm::cos(glm::radians(yaw));
	float y = glm::sin(glm::radians(pitch));
	float z = glm::cos(glm::radians(pitch)) * glm::sin(glm::radians(yaw));
	SetTo(GetPos() + glm::vec3{ x, y, z });
}
