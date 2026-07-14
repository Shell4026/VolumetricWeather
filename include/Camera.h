#pragma once

#include "glm/glm.hpp"
class Camera
{
public:
	Camera() = default;
	virtual ~Camera() = default;

	virtual void SetPos(const glm::vec3& pos) { this->pos = pos; }
	virtual void SetTo(const glm::vec3& to) { this->to = to; }
	virtual void SetUp(const glm::vec3& up) { this->up = up; }
	void SetWidth(float width) { this->width = width; }
	void SetHeight(float height) { this->height = height; }
	virtual void SetNear(float nearPlane) { this->nearPlane = nearPlane; }
	virtual void SetFar(float farPlane) { this->farPlane = farPlane; }
	virtual void SetOrtho() { bOrtho = true; }
	virtual void SetPerspective() { bOrtho = false; }

	virtual void UpdateMatrix();

	auto GetPos() const -> const glm::vec3& { return pos; }
	auto GetTo() const -> const glm::vec3& { return to; }
	auto GetUp() const -> const glm::vec3& { return up; }
	auto GetMatrixProj() const -> const glm::mat4& { return proj; }
	auto GetMatrixView() const -> const glm::mat4& { return view; }
	auto GetWidth() const -> float { return width; }
	auto GetHeight() const -> float { return height; }
	auto GetNear() const -> float { return nearPlane; }
	auto GetFar() const -> float { return farPlane; }
	auto IsOrtho() const -> bool { return bOrtho; }
	auto IsPerspective() const -> bool { return !bOrtho; }
private:
	float fov = 60.f;
	float fovRadians = glm::radians(fov);
	float width = 1024.f;
	float height = 768.f;
	float nearPlane = 0.1f;
	float farPlane = 1000.f;
	glm::vec3 pos{ 1.f, 1.f, 1.f };
	glm::vec3 to{ 0.f, 0.f, 0.f };
	glm::vec3 up{ 0.f, 1.f, 0.f };

	glm::mat4 proj{ 1.f };
	glm::mat4 view{ 1.f };

	bool bOrtho = false;
};