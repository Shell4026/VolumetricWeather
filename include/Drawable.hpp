#pragma once
#include "glm/mat4x4.hpp"

class AMeshBase;
class Material;

struct Drawable
{
	glm::mat4 modelMatrix{ 1.f };
	const Material* mat = nullptr;
	const AMeshBase* mesh = nullptr;
};