#include "Camera.h"

#include "glm/gtc/matrix_transform.hpp"
void Camera::UpdateMatrix()
{
	if (!bOrtho)
		proj = glm::perspectiveFovRH_ZO(fovRadians, width, height, nearPlane, farPlane);
	else
		proj = glm::orthoRH_ZO(-width / 2, width / 2, -height / 2, height / 2, nearPlane, farPlane);
	view = glm::lookAtRH(pos, to, up);
}