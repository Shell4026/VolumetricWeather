#pragma once
#include "Mesh.h"

#include "glm/glm.hpp"

struct Drawable
{
	glm::mat4 modelMatrix{ 1.f };
	AMeshBase* mesh = nullptr;
};