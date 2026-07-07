#pragma once
#include "Event.h"
#include <bitset>
class Input
{
	friend class Window;
public:
	static auto IsKeyDown(Event::KeyType keycode) -> bool;
private:
	static std::bitset<100> keyPressing;
};