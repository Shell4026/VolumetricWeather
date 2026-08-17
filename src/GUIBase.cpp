#include "GUIBase.h"

#include <imgui/imgui.h>
#include <imgui/imgui_stdlib.h>

#include <algorithm>
GUIBase::GUIBase(std::string name, uint32_t width, uint32_t height) :
	name(std::move(name)), width(width), height(height)
{
}

void GUIBase::RenderGUI()
{
	ImGui::SetNextWindowSize(ImVec2{ static_cast<float>(width), static_cast<float>(height) }, ImGuiCond_::ImGuiCond_Once);
	if (ImGui::Begin(name.c_str(), nullptr, ImGuiWindowFlags_::ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			for (int i = 0; i < children.size(); ++i)
			{
				Child& child = children[i];
				if (child.displayType != DisplayType::Menu)
					continue;
				if (ImGui::Button(child.guiPtr->GetName().c_str()))
					menu = i;
			}
			ImGui::EndMenuBar();
		}
		if (menu != -1)
		{
			children[menu].guiPtr->RenderGUI();
			ImGui::Separator();
		}
		for (Child& child : children)
		{
			if (child.displayType == DisplayType::Default)
				child.guiPtr->RenderGUI();
		}
	}
	ImGui::End();
}

void GUIBase::AddChild(IGUI& gui, DisplayType type)
{
	auto it = std::remove_if(children.begin(), children.end(),
		[&](const Child& child)
		{
			return child.guiPtr != nullptr && child.guiPtr == &gui;
		}
	);
	if (it != children.end())
		return;
	Child& child = children.emplace_back();
	child.guiPtr = &gui;
	child.displayType = type;
}

void GUIBase::RemoveChild(IGUI& gui)
{
	auto it = std::remove_if(children.begin(), children.end(),
		[&](const Child& child)
		{
			return child.guiPtr != nullptr && child.guiPtr == &gui;
		}
	);
	children.erase(it, children.end());
}

void GUIBase::RemoveChild(std::string_view name)
{
	auto it = std::remove_if(children.begin(), children.end(),
		[&](const Child& child)
		{
			return child.guiPtr != nullptr && child.guiPtr->GetName() == name;
		}
	);
	children.erase(it, children.end());
}

auto GUIBase::GetChild(std::string_view name) -> IGUI*
{
	auto it = std::find_if(children.begin(), children.end(),
		[&](const Child& child)
		{
			return child.guiPtr != nullptr && child.guiPtr->GetName() == name;
		}
	);
	if (it == children.end())
		return nullptr;
	return it->guiPtr;
}
