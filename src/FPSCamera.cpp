#include "FPSCamera.h"
#include "core/Logger.h"

FPSCamera::FPSCamera()
{
	quat = glm::quat{ glm::radians(glm::vec3{0.f, pitch, 0.f}) };
}

void FPSCamera::SetPos(const glm::vec3& pos)
{
	Camera::SetPos(pos);
	SetTo(GetPos() + forward);
}

void FPSCamera::SetTo(const glm::vec3& to)
{
	Camera::SetTo(to);
	forward = glm::normalize(to - GetPos());
}

void FPSCamera::SetYaw(float degree)
{
	yaw = degree;
	yaw = std::fmodf(yaw, 360.f);
}

void FPSCamera::AddYaw(float degree)
{
	yaw += degree;
	yaw = std::fmodf(yaw, 360.f);

	const glm::quat rot{ glm::radians(glm::vec3{ 0.f, degree, 0.f }) };
	quat = rot * quat;
	const glm::vec3 up = rot * GetUp();
	forward = glm::normalize(rot * forward);
	SetUp(up);
	Camera::SetTo(GetPos() + forward);
}

void FPSCamera::SetPitch(float degree)
{
	pitch = degree;
	pitch = std::fmodf(pitch, 360.f);
	const glm::quat quat{ glm::vec3{ pitch, 0.f, 0.f } };
}

void FPSCamera::AddPitch(float degree)
{
	pitch += degree;
	pitch = std::fmodf(pitch, 360.f);

	glm::vec3 up = GetUp();
	const glm::vec3 right = glm::cross(forward, up);
	glm::quat rot = glm::angleAxis(glm::radians(degree), right);
	quat = rot * quat;
	forward = glm::normalize(rot * forward);
	up = glm::cross(right, forward);
	SetUp(up);
	Camera::SetTo(GetPos() + forward);
}