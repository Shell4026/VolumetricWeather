#pragma once
#include "Scene.h"
#include "GLBLoader.h"
#include "Camera.h"
#include "PresetManager.h"
#include "GUIBase.h"

#include "core/CircularQueue.hpp"

#include "render/VulkanContext.h"
#include "render/Drawable.hpp"
#include "render/Shader.h"

#include "weather/AtmosphereRMSEMeasurement.h"
#include "weather/Setting.h"
#include "weather/ArtistGUI.h"

#include <glm/gtc/quaternion.hpp>

#include <memory>
#include <map>
class ShadowPass;
class OpaquePass;
class LowDepthPass;
class LUTPass;
class AtmosphereBasePass;
class AtmospherePass;
class HillairePass;
class PostProcessPass;
class BlitPass;
class CloudPass;
class CloudTRPass;
class CloudEditor;

class Material;
class VulkanImage;
class BasisScene : public AScene
{
public:
	BasisScene(VulkanContext& ctx, const ImGUI& imgui, Window& window, SamplerManager& samplerManager);
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

	auto GetActivePassList() -> std::vector<APass*> & override;
	void BeginBuildCommandBuffer() override;
private:
	void DrawDebugGUI();
	void DrawOverlay();
	void DrawPresetGUI();
	void SetAtmosphereModel(bool bHillare);
	void CreateDrawables();
	void CreateCityDrawables();
	void ControlCamera(double dt);
	void UploadSettingsToGPU();
	void UpdateSun();
	void UpdateOpaqueMaterialData();
private:
	VkSampler sampler = VK_NULL_HANDLE;

	std::unique_ptr<ShadowPass> shadowPass;
	std::unique_ptr<OpaquePass> opaquePass;
	std::unique_ptr<LowDepthPass> lowDepthPass;
	std::unique_ptr<LUTPass> lutPass;
	std::unique_ptr<AtmospherePass> atmospherePass;
	std::unique_ptr<HillairePass> hillairePass;
	std::unique_ptr<CloudPass> cloudPass;
	std::unique_ptr<CloudTRPass> cloudTRPass;
	std::unique_ptr<PostProcessPass> postProcessPass;
	std::unique_ptr<BlitPass> blitPass;
	AtmosphereRMSEMeasurement rmseMeasurement;

	AtmosphereBasePass* currentAtmospherePass = nullptr;

	Shader opaqueShader;

	struct Mountain
	{
		GLBLoader::Model model;
		struct MaterialData
		{
			alignas(16) glm::vec4 sun;
			alignas(16) glm::mat4 viewProj;
			float atmosphereRadius = 6'460'000.f;
			float groundRadius = 6'360'000.f;
		} data;
		std::unique_ptr<Material> material;
	} mountain;
	struct City
	{
		GLBLoader::Model model;
		struct MaterialData
		{
			alignas(16) glm::vec4 sun;
			alignas(16) glm::mat4 viewProj;
			float atmosphereRadius = 6'460'000.f;
			float groundRadius = 6'360'000.f;
		} data;
		std::vector<std::unique_ptr<Material>> materials;
	} city;
	std::unique_ptr<VulkanImage> blueNoise;

	std::vector<APass*> allPasses;
	std::vector<APass*> activePasses;
	std::vector<APass*> activePasses2;
	std::vector<Drawable> drawables;

	CircularQueue<double, 10> shadowPassElapsed;
	CircularQueue<double, 10> opaquePassElapsed;
	CircularQueue<double, 10> lowDepthPassElapsed;

	CircularQueue<double, 10> transmittanceLUTPassElapsed;
	CircularQueue<double, 10> msLUTPassElapsed;
	CircularQueue<double, 10> skyViewLUTPassElapsed;
	CircularQueue<double, 10> aerialPerspectiveLUTPassElapsed;
	CircularQueue<double, 10> aerialShadowLUTPassElapsed;

	CircularQueue<double, 10> atmospherePassElapsed;
	CircularQueue<double, 10> cloudPassElapsed;
	CircularQueue<double, 10> postProcessPassElapsed;

	uint64_t counter = 0;
	int menu = 0;
	int artistMenu = 0;

	struct Preset
	{
		glm::vec3 camPos;
		glm::quat camQuat;
		Json artistSetting;
		auto Serialize() const -> Json;
		void Deserialize(const Json& json);
	};
	std::vector<Preset> presets;
	PresetManager<Preset> presetManager;

	struct ImageReCreateRequest
	{
		const VulkanImage* img;
		uint32_t width;
		uint32_t height;
	};
	struct ChangeAtmosphereRequest
	{
		bool bValid = false;
		bool bHillaire = false;
	} changeAtmosphereReq;
	std::map<const VulkanImage*, ImageReCreateRequest> imgRecreateRequests;

	std::unique_ptr<CloudEditor> cloudEditor;

	WeatherSetting setting;
	ArtistSetting artistSetting;
	WeatherDirtyFlags settingDirtyFlags = 0;

	GUIBase gui;
	ArtistGUI artistGUI;
	bool bCloudEnable = true;
};
