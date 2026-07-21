#pragma once
#include "Scene.h"
#include "GLBLoader.h"
#include "Camera.h"
#include "AtmosphereRMSEMeasurement.h"

#include "core/CircularQueue.hpp"

#include "render/VulkanContext.h"
#include "render/Drawable.hpp"
#include "render/Shader.h"

#include <memory>
#include <map>
class ShadowPass;
class OpaquePass;
class LUTPass;
class AtmospherePass;
class HillairePass;
class PostProcessPass;
class BlitPass;
class Material;
class VulkanImage;
class BasisScene : public AScene
{
public:
	BasisScene(VulkanContext& ctx, const ImGUI& imgui, Window& window);
	~BasisScene();

	void Clear() override;

	/// @brief 이 시점에선 아직 GPU에서 쓰고 있을 수 있기 때문에 렌더링 리소스를 업데이트 해서는 안 됨
	void Update(double dt) override;
	void BeginRender(double dt) override;
	void Render(double dt) override;
protected:
	auto CreateSceneCamera() -> std::unique_ptr<Camera> override;
	void PrepareResource() override;
	void SetupPass() override;

	auto GetActivePassList() -> std::vector<APass*> & override { return activePasses; }
	void BeginBuildCommandBuffer() override;
private:
	void DrawDebugGUI();
	void SetAtmosphereModel(bool useHillaire);
	void CreateDrawables();
	void ControlCamera(double dt);
	void UpdateSun();
private:
	VkSampler sampler = VK_NULL_HANDLE;

	std::unique_ptr<ShadowPass> shadowPass;
	std::unique_ptr<OpaquePass> opaquePass;
	std::unique_ptr<LUTPass> lutPass;
	std::unique_ptr<AtmospherePass> atmospherePass;
	std::unique_ptr<HillairePass> hillairePass;
	std::unique_ptr<PostProcessPass> postProcessPass;
	std::unique_ptr<BlitPass> blitPass;
	AtmosphereRMSEMeasurement rmseMeasurement;

	AtmospherePass* currentAtmospherePass = nullptr;

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

	std::vector<APass*> allPasses;
	std::vector<APass*> activePasses;
	std::vector<Drawable> drawables;

	glm::vec4 sun{ -1.f, 0.f, -1.f, 15.f };

	CircularQueue<double, 10> shadowPassElapsed;
	CircularQueue<double, 10> opaquePassElapsed;
	CircularQueue<double, 10> transmittanceLUTPassElapsed;
	CircularQueue<double, 10> atmospherePassElapsed;
	CircularQueue<double, 10> postProcessPassElapsed;

	uint64_t counter = 0;
	int menu = 0;

	struct ImageReCreateRequest
	{
		const VulkanImage* img;
		uint32_t width;
		uint32_t height;
	};
	std::map<const VulkanImage*, ImageReCreateRequest> imgRecreateRequests;
	bool bChangeAtmosphereModelRequest = false;
};
