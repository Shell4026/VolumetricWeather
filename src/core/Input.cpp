#include "core/Input.h"

std::bitset<100> Input::keyPressing{};

auto Input::IsKeyDown(Event::KeyType keycode) -> bool
{
	return keyPressing[static_cast<int>(keycode)];
}
