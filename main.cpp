#include "core/Window.h"
#include "core/Logger.h"
#include "core/ImGUI.h"

#include "render/VulkanContext.h"
#include "render/SamplerManager.h"

#include "BasisScene.h"
#include "SceneManager.h"

#include <iostream>
#include <chrono>
int main()
{
	bool bPauseRendering = false;

	Window win{};
	win.Open(1920, 1080);

	VulkanContext ctx{ win };
	ctx.Init();

	SamplerManager samplerManager{ ctx };
	
	ImGUI imgui{ win, ctx };
	imgui.Init();
	win.AddEventHook(
		[&](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			imgui.ProcessEvent(msg, wParam, lParam);
			if (msg == WM_SIZE)
			{
				if (wParam == SIZE_MINIMIZED)
				{
					bPauseRendering = true;
					SH_INFO("Pause");
				}
				else
				{
					bPauseRendering = false;
					ctx.ReSize();
				}
			}
		}
	);
	BasisScene scene{ ctx, imgui, win, samplerManager };
	scene.Init();
	SceneManager::AddScene("main", scene);
	SceneManager::SetCurrentScene(scene);
	double dt = 0.0;
	while (win.IsOpen())
	{
		auto start = std::chrono::steady_clock::now();

		win.Update();
		imgui.Begin(dt);
		ImGui::ShowDemoWindow();
		SceneManager::UpdateCurrentScene(dt);
		imgui.End();
		SceneManager::RenderCurrentScene(dt, bPauseRendering);

		auto end = std::chrono::steady_clock::now();
		dt = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
		dt /= 1'000'000;
	}
	scene.Clear();
	imgui.Clear();
	samplerManager.Clear();
	return 0;
}
