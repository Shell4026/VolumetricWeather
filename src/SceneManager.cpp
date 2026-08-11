#include "SceneManager.h"

std::unordered_map<std::string, AScene*> SceneManager::scenes;
AScene* SceneManager::currentScene = nullptr;

void SceneManager::AddScene(const std::string& name, AScene& scene)
{
	auto it = scenes.find(name);
	if (it != scenes.end())
		return;

	scenes.insert({ name, &scene });
}

void SceneManager::SetCurrentScene(AScene& scene)
{
	currentScene = &scene;
}

void SceneManager::UpdateCurrentScene(double dt)
{
	if (currentScene == nullptr)
		return;
	currentScene->Update(dt);
}

void SceneManager::RenderCurrentScene(double dt, bool bPause)
{
	if (currentScene == nullptr || bPause)
		return;
	currentScene->BeginRender(dt);
	currentScene->Render(dt);
}

auto SceneManager::GetScene(const std::string& name) -> AScene*
{
	auto it = scenes.find(name);
	if (it == scenes.end())
		return nullptr;
	return it->second;
}
