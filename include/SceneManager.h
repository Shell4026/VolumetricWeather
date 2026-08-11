#pragma once
#include "Scene.h"

#include <vector>
#include <string>
class SceneManager
{
public:
	static void AddScene(const std::string& name, AScene& scene);
	static void SetCurrentScene(AScene& scene);
	static void UpdateCurrentScene(double dt);
	static void RenderCurrentScene(double dt, bool bPause);

	static auto GetScene(const std::string& name) -> AScene*;
	static auto GetCurrentScene() -> AScene* { return currentScene; }
private:
	static std::unordered_map<std::string, AScene*> scenes;

	static AScene* currentScene;
};