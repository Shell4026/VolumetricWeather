#pragma once
#include "Scene.h"
#include "GLBLoader.h"
#include "Camera.h"

#include "core/CircularQueue.hpp"

#include "render/VulkanContext.h"
#include "render/Drawable.hpp"
#include "render/Shader.h"

#include <memory>
class OpaquePass;
class AtmospherePass;
class PostProcessPass;
class ShadowPass;
class Material;
class BasisScene : public AScene
{
public:
	BasisScene(VulkanContext& ctx, const ImGUI& imgui, Window& window);
	~BasisScene();

	void Clear() override;

	void Update(double dt) override;
	void Render(double dt) override;
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
	void UpdateSun();
private:
	VkSampler sampler = VK_NULL_HANDLE;

	std::unique_ptr<ShadowPass> shadowPass;
	std::unique_ptr<OpaquePass> opaquePass;
	std::unique_ptr<AtmospherePass> atmospherePass;
	std::unique_ptr<PostProcessPass> postProcessPass;

	Shader opaqueShader;

	struct Mountain
	{
		GLBLoader::Model model;
		struct MaterialData
		{
			alignas(16) glm::vec4 sun;
			alignas(16) glm::mat4 viewProj;
		} data;
		std::unique_ptr<Material> material;
	} mountain;

	std::vector<APass*> activePasses;
	std::vector<Drawable> drawables;

	glm::vec4 sun{ -1.f, 0.f, -1.f, 15.f };

	CircularQueue<double, 10> shadowPassElapsed;
	CircularQueue<double, 10> opaquePassElapsed;
	CircularQueue<double, 10> atmospherePassElapsed;
	CircularQueue<double, 10> postProcessPassElapsed;

	uint64_t counter = 0;
};