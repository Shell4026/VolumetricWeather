#pragma once
#include <string>
#include <string_view>
class IGUI
{
public:
	virtual ~IGUI() = default;

	virtual void RenderGUI() = 0;
	virtual auto GetName() const -> const std::string & = 0;
};