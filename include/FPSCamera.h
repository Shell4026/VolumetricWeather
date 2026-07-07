#pragma once
#include "Camera.h"

class FPSCamera : public Camera
{
public:
	FPSCamera();

	void SetYaw(float degree);
	void AddYaw(float degree);
	void SetPitch(float degree);
	void AddPitch(float degree);
	void CalcTo();
private:
	float yaw = 0.f;
	float pitch = 0.f;
};