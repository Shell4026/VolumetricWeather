#include "Camera.h"

#include "glm/gtc/matrix_transform.hpp"
void Camera::UpdateMatrix()
{
	if (bUpdateProj)
	{
		if (!bOrtho)
			proj = glm::perspectiveFovRH_ZO(fovRadians, width, height, nearPlane, farPlane);
		else
			proj = glm::orthoRH_ZO(-width / 2, width / 2, -height / 2, height / 2, nearPlane, farPlane);
		invProj = glm::inverse(proj);
	}

	if (bUpdateView)
	{
		view = glm::lookAtRH(pos, to, up);
		invView = glm::inverse(view);
	}

	bUpdateProj = false;
	bUpdateView = false;
}