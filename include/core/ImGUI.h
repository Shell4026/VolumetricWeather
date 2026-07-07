#pragma once
#include "imgui/imgui.h"
#include <Windows.h>

class Window;
class VulkanContext;
class ImGUI
{
public:
	ImGUI(Window& window, VulkanContext& ctx);
	~ImGUI();

	void Init();
	void Clear();

	void Begin(double dt);
	void Render();
	void End();

	void ProcessEvent(UINT msg, WPARAM wParam, LPARAM lParam);
private:
	Window& window;
	VulkanContext& ctx;

	double dt = 0.0;

	bool bInit = false;
};