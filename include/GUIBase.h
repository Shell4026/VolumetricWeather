#pragma once
#include "IGUI.h"

#include <vector>
#include <string>
#include <string_view>
class GUIBase : public IGUI
{
public:
	enum class DisplayType
	{
		Default,
		Menu
	};
public:
	GUIBase(std::string name, uint32_t width, uint32_t height);

	void RenderGUI() override;

	void AddChild(IGUI& gui, DisplayType type = DisplayType::Default);
	void RemoveChild(IGUI& gui);
	void RemoveChild(std::string_view name);
	auto GetChild(std::string_view name) -> IGUI*;

	auto GetName() const -> const std::string& { return name; }
private:
	std::string name;
	struct Child
	{
		IGUI* guiPtr = nullptr;
		DisplayType displayType = DisplayType::Default;
	};
	std::vector<Child> children;

	uint32_t width = 1;
	uint32_t height = 1;
	int menu = -1;
};