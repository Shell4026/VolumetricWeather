#include "Window.h"
#include "VulkanContext.h"
#include "Scene.h"
#include "Logger.h"

#include <iostream>
#include <chrono>
int main()
{
	Window win{};
	win.Open();

	VulkanContext ctx{ win };
	ctx.Init();
	
	Scene scene{ ctx };
	scene.Init();

	double dt = 0.0;
	while (win.IsOpen())
	{
		auto start = std::chrono::steady_clock::now();

		win.Update();
		scene.Render(dt);
		
		auto end = std::chrono::steady_clock::now();
		dt = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
		dt /= 1'000'000;
	}

	scene.Clear();
	return 0;
}
