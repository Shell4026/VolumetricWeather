#include "Window.h"
#include "VulkanContext.h"
#include "Scene.h"

#include <iostream>
int main()
{
	Window win{};
	win.Open();

	VulkanContext ctx{ win };
	ctx.Init();
	
	Scene scene{ ctx };
	scene.Init();

	while (win.IsOpen())
	{
		win.Update();
		scene.Render();
	}

	scene.Clear();
	return 0;
}
