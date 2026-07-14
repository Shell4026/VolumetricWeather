#pragma once
#include "render/Drawable.hpp"

class IDrawablePass
{
public:
	virtual ~IDrawablePass() = default;

	virtual void PushDrawable(const Drawable& mesh) = 0;
};