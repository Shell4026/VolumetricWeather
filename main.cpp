#include "Window.h"
#include "VulkanContext.h"
#include "Scene.h"
#include "Logger.h"
#include "ImGUI.h"

#include <iostream>
#include <chrono>
int main()
{
	Window win{};
	win.Open();

	VulkanContext ctx{ win };
	ctx.Init();
	
	ImGUI imgui{ win, ctx };
	imgui.Init();
	win.AddEventHook(
		[&](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			imgui.ProcessEvent(msg, wParam, lParam);
		}
	);
	Scene scene{ ctx, imgui, win };
	scene.Init();

	double dt = 0.0;
	while (win.IsOpen())
	{
		auto start = std::chrono::steady_clock::now();

		win.Update();
		imgui.Begin(dt);
		ImGui::ShowDemoWindow();
		imgui.End();
		scene.Render(dt);

		auto end = std::chrono::steady_clock::now();
		dt = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
		dt /= 1'000'000;
	}

	scene.Clear();
	imgui.Clear();
	return 0;
}
