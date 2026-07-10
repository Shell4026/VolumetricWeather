#pragma once
#include "Scene.h"
#include "GLBLoader.h"

#include "render/VulkanContext.h"
#include "render/Drawable.hpp"
#include "render/Shader.h"

#include <memory>

class AtmospherePass;
class CompositePass;
class Material;
class BasisScene : public AScene
{
public:
	BasisScene(VulkanContext& ctx, const ImGUI& imgui, Window& window);
	~BasisScene();

	void Clear() override;

	void Update(double dt) override;
protected:
	auto CreateSceneCamera() -> std::unique_ptr<Camera> override;
	void PrepareResource() override;
	void SetupPass() override;

	auto GetActivePassList() -> std::vector<APass*> & override { return activePasses; }
	void BeginBuildCommandBuffer() override;
private:
	void DrawDebugGUI();
	void CreateDrawables();
	void ControlCamera(double dt);
private:
	VkSampler sampler = VK_NULL_HANDLE;

	std::unique_ptr<OpaquePass> opaquePass;
	std::unique_ptr<AtmospherePass> atmospherePass;
	std::unique_ptr<CompositePass> compositePass;

	Shader opaqueShader;

	struct Mountain
	{
		GLBLoader::Model model;
		struct MaterialData
		{
			glm::vec4 color{ 1.f, 1.f, 1.f, 1.f };
		} data;
		std::unique_ptr<Material> material;
	} mountain;

	std::vector<APass*> activePasses;
	std::vector<Drawable> drawables;
};