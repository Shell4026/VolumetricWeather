#include "FPSCamera.h"
#include "core/Logger.h"

FPSCamera::FPSCamera()
{
	quat = glm::quat{ glm::radians(glm::vec3{0.f, yaw, 0.f}) };
	forward = quat * forward;
	SetUp(quat * glm::vec3{ 0.f, 1.f, 0.f });
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

	glm::vec3 rot = glm::eulerAngles(quat);
	rot.y = glm::radians(yaw);
	quat = glm::quat{ rot };

	ApplyQuat();
}

void FPSCamera::AddYaw(float degree)
{
	yaw += degree;
	yaw = std::fmodf(yaw, 360.f);

	const glm::quat rot{ glm::radians(glm::vec3{ 0.f, degree, 0.f }) };
	quat = rot * quat;

	ApplyQuat();
}

void FPSCamera::SetPitch(float degree)
{
	pitch = degree;
	pitch = std::fmodf(pitch, 360.f);

	glm::vec3 rot = glm::eulerAngles(quat);
	rot.x = glm::radians(pitch);
	quat = glm::quat{ rot };

	ApplyQuat();
}

void FPSCamera::AddPitch(float degree)
{
	pitch += degree;
	pitch = std::fmodf(pitch, 360.f);

	const glm::vec3 right = glm::cross(forward, GetUp());
	const glm::quat rot = glm::angleAxis(glm::radians(degree), right);
	quat = rot * quat;

	ApplyQuat();
}

void FPSCamera::SetQuat(const glm::quat quat)
{
	this->quat = quat;
	ApplyQuat();
}

void FPSCamera::ApplyQuat()
{
	forward = forward = quat * glm::vec3{ 0.f, 0.f, -1.f };
	SetUp(quat * glm::vec3{ 0.f, 1.f, 0.f });
	Camera::SetTo(GetPos() + forward);
}
