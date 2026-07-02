#include "Camera.h"

#include "glm/gtc/matrix_transform.hpp"
void Camera::UpdateMatrix()
{
	proj = glm::perspectiveFovRH_ZO(fovRadians, width, height, nearPlane, farPlane);
	view = glm::lookAtRH(pos, to, up);
}