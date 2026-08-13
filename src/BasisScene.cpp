#include "BasisScene.h"
#include "FPSCamera.h"
#include "TextureLoader.h"
#include "CloudEditor.h"

#include "core/Input.h"
#include "core/Window.h"

#include "render/Material.h"

#include "pass/ShadowPass.h"
#include "pass/OpaquePass.h"
#include "pass/LowDepthPass.h"
#include "pass/LUTPass.h"
#include "pass/AtmospherePass.h"
#include "pass/HillairePass.h"
#include "pass/CloudPass.h"
#include "pass/CloudTRPass.h"
#include "pass/PostProcessPass.h"
#include "pass/BlitPass.h"
#include "pass/CloudPaintPass.h"

#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"
#include "glm/gtc/quaternion.hpp"

#include <queue>
#include <string>
#include <format>
BasisScene::BasisScene(VulkanContext& ctx, const ImGUI& imgui, Window& window, SamplerManager& samplerManager) :
	AScene(ctx, imgui, window, samplerManager)
{
	const glm::quat q = glm::quat{ glm::vec3(0.f, 0.f, glm::radians(0.f)) };
	const glm::vec3 sunDir = q * glm::normalize(glm::vec3{ -1.f, 0.f, -1.f });
	sun = glm::vec4{ sunDir, sun.w };

	presetManager.LoadPresets("presets.json");
}

BasisScene::~BasisScene()
{
	Clear();
}

void BasisScene::Clear()
{
	const VkDevice device = ctx.GetDevice();
	vkDeviceWaitIdle(device);

	cloudEditor.reset();

	for (APass* pass : allPasses)
		pass->Clear();
	allPasses.clear();
	activePasses.clear();

	opaqueShader.Clear();

	mountain = Mountain{};
	city = City{};

	if (sampler != VK_NULL_HANDLE)
	{
		vkDestroySampler(device, sampler, nullptr);
		sampler = VK_NULL_HANDLE;
	}
	AScene::Clear();
}

void BasisScene::Update(double dt)
{
	// 메모) 이 시점에선 아직 GPU에서 쓰고 있을 수 있기 때문에 렌더링 리소스를 업데이트 해서는 안 됨
	if (!rmseMeasurement.IsRunning())
		ControlCamera(dt);
	DrawDebugGUI();
	cloudEditor->Update();
}

void BasisScene::BeginRender(double dt)
{
	AScene::BeginRender(dt);
	if (changeAtmosphereReq.bValid)
	{
		SetAtmosphereModel(changeAtmosphereReq.bHillaire);
		changeAtmosphereReq.bValid = false;
	}
	const bool bImgRecreate = !imgRecreateRequests.empty();
	for (const auto& [img, req] : imgRecreateRequests)
	{
		if (img == lutPass->GetSkyViewLUT())
		{
			InvalidateImageUsage(img->GetImage());
			lutPass->ReCreateSkyViewLUT(req.width, req.height);
		}
		else if (img == lutPass->GetAerialShadowLUT())
		{
			InvalidateImageUsage(img->GetImage());
			lutPass->ReCreateShadowLUT(req.width, req.height);
		}
	}
	imgRecreateRequests.clear();
	if (bImgRecreate)
		hillairePass->UpdateMaterial();

	if (const std::optional<bool> requestedModel = rmseMeasurement.Update(
		*blitPass, *atmospherePass->GetOutputImage(), *hillairePass->GetOutputImage()))
	{
		SetAtmosphereModel(*requestedModel);
		if (const auto& result = rmseMeasurement.GetResult())
		{
			SH_INFO_FORMAT("Atmosphere RGB RMSE: {} (R: {}, G: {}, B: {}, pixels: {})",
				result->rmse,
				result->channelRMSE.r,
				result->channelRMSE.g,
				result->channelRMSE.b,
				result->pixelCount);
		}
	}

	if (counter > 0)
	{
		shadowPassElapsed.Push(shadowPass->GetElapsedTimeMs());
		opaquePassElapsed.Push(opaquePass->GetElapsedTimeMs());
		if (currentAtmospherePass == hillairePass.get())
		{
			lowDepthPassElapsed.Push(lowDepthPass->GetElapsedTimeMs());
			transmittanceLUTPassElapsed.Push(lutPass->GetLUTElpasedTimeMs(LUTPass::LUTType::Transmittance));
			msLUTPassElapsed.Push(lutPass->GetLUTElpasedTimeMs(LUTPass::LUTType::MultipleScattering));
			skyViewLUTPassElapsed.Push(lutPass->GetLUTElpasedTimeMs(LUTPass::LUTType::SkyView));
			aerialPerspectiveLUTPassElapsed.Push(lutPass->GetLUTElpasedTimeMs(LUTPass::LUTType::AerialPerspective));
			aerialShadowLUTPassElapsed.Push(lutPass->GetLUTElpasedTimeMs(LUTPass::LUTType::AerialShadow));
			if (bCloudEnable)
				cloudPassElapsed.Push(cloudPass->GetElapsedTimeMs());
		}
		atmospherePassElapsed.Push(currentAtmospherePass->GetElapsedTimeMs());
		postProcessPassElapsed.Push(postProcessPass->GetElapsedTimeMs());
	}
}

void BasisScene::Render(double dt)
{
	AScene::Render(dt);
	++counter;
}

auto BasisScene::CreateSceneCamera() -> std::unique_ptr<Camera>
{
	std::unique_ptr<FPSCamera> camPtr = std::make_unique<FPSCamera>();
	camPtr->SetWidth(window.GetWidth());
	camPtr->SetHeight(window.GetHeight());
	camPtr->SetFar(100000.0f);
	camPtr->SetPos(glm::vec3{ 0.f, 100.f, 0.f });
	camPtr->SetYaw(-90.f);
	camPtr->SetPitch(0.f);
	camPtr->UpdateMatrix();
	UpdateCameraData();

	return camPtr;
}

void BasisScene::PrepareResource()
{
	AScene::PrepareResource();

	// 산 모델용 샘플러 생성
	VkSamplerCreateInfo samplerCi = VulkanSampler::GetCreateInfo();
	VK_RESULT_CHECK(vkCreateSampler(ctx.GetDevice(), &samplerCi, nullptr, &sampler));
	SH_INFO_FORMAT("ctx: {}, instance: {}", (void*)&ctx, (void*)ctx.GetInstance());
	// opaqueShader 초기화
	std::vector<VkDescriptorSetLayoutBinding> set1Bindings;
	VkDescriptorSetLayoutBinding& binding0 = set1Bindings.emplace_back();
	binding0.binding = 0;
	binding0.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT | VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
	binding0.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding0.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding1 = set1Bindings.emplace_back();
	binding1.binding = 1;
	binding1.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
	binding1.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding1.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding2 = set1Bindings.emplace_back();
	binding2.binding = 2;
	binding2.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
	binding2.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding2.descriptorCount = 1;

	VkPushConstantRange pc{};
	pc.size = sizeof(glm::mat4);
	pc.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;

	opaqueShader.
		AddSet(0, GetCameraDescriptorSetLayout()).
		AddSet(1, std::move(set1Bindings)).
		Build(ctx.GetDevice(), "shaders/mesh.vert.spv", "shaders/mesh.frag.spv", &pc);

	// 산
	mountain.model = GLBLoader::LoadGLB(ctx, "models/mountain_1.glb");

	mountain.material = std::make_unique<Material>(ctx, opaqueShader);
	mountain.material->
		AddBinding<Mountain::MaterialData>(0).
		AddBinding(1, mountain.model.textures[0], sampler).
		AddBinding(2, *ctx.GetEmptyImage(), sampler).
		Build(GetDescriptorPool());

	mountain.data.sun = sun;
	mountain.material->UpdateBindingData(0, mountain.data);

	// 도시
	city.model = GLBLoader::LoadGLB(ctx, "models/city.glb");
	city.data.sun = sun;
	for (std::size_t i = 0; i < city.model.textures.size(); ++i)
	{
		Material* const matPtr = city.materials.emplace_back(std::make_unique<Material>(ctx, opaqueShader)).get();
		matPtr->
			AddBinding<City::MaterialData>(0).
			AddBinding(1, city.model.textures[i], sampler).
			AddBinding(2, *ctx.GetEmptyImage(), sampler).
			Build(GetDescriptorPool());
		matPtr->UpdateBindingData(0, city.data);
	}
	CreateCityDrawables();
	//CreateDrawables();

	// 텍스쳐
	std::optional<VulkanImage> texOpt = TextureLoader::Load(ctx, "textures/BlueNoise.png");
	if (!texOpt.has_value())
		throw std::runtime_error{ "textures/BlueNoise.png is not loaded!" };
	blueNoise = std::make_unique<VulkanImage>(std::move(texOpt.value()));

	// 에디터
	cloudEditor = std::make_unique<CloudEditor>(*this);
	allPasses.push_back(cloudEditor->GetPass());
}

void BasisScene::SetupPass()
{
	const uint32_t width = ctx.GetSwapChainExtent().width;
	const uint32_t height = ctx.GetSwapChainExtent().height;
	shadowPass = std::make_unique<ShadowPass>();
	shadowPass->Init(ctx, samplerManager, GetDescriptorPool(), VK_NULL_HANDLE);
	mountain.material->UpdateBindingData(2, *shadowPass->GetShadowMap(), shadowPass->GetShadowSampler()->GetSampler());
	for (std::unique_ptr<Material>& matPtr : city.materials)
	{
		if (matPtr == nullptr)
			continue;
		matPtr->UpdateBindingData(2, *shadowPass->GetShadowMap(), shadowPass->GetShadowSampler()->GetSampler());
	}
	opaquePass = std::make_unique<OpaquePass>();
	opaquePass->SetShader(opaqueShader);
	opaquePass->SetImageSize(width, height);
	opaquePass->Init(ctx, samplerManager, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	lowDepthPass = std::make_unique<LowDepthPass>();
	lowDepthPass->SetDepthTexture(*opaquePass->GetOutputImageDepth());
	lowDepthPass->Init(ctx, samplerManager, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	lutPass = std::make_unique<LUTPass>();
	lutPass->SetDepthTexture(*opaquePass->GetOutputImageDepth());
	lutPass->SetShadowMap(*shadowPass->GetShadowMap());
	lutPass->SetShadowSampler(*shadowPass->GetShadowSampler());
	lutPass->SetNoiseTexture(*blueNoise);
	lutPass->Init(ctx, samplerManager, GetDescriptorPool(), GetCameraDescriptorSetLayout());
	lutPass->UpdateLUTFlags(LUTPass::LUTType::Transmittance);

	atmospherePass = std::make_unique<AtmospherePass>();
	atmospherePass->SetOpaqueTexture(*opaquePass->GetOutputImage());
	atmospherePass->SetOpaqueDepthTexture(*opaquePass->GetOutputImageDepth());
	atmospherePass->SetShadowMap(*shadowPass->GetShadowMap());
	atmospherePass->SetShadowSampler(*shadowPass->GetShadowSampler());
	atmospherePass->SetImageSize(width, height);
	atmospherePass->Init(ctx, samplerManager, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	cloudPass = std::make_unique<CloudPass>();
	cloudPass->SetSceneDepthTexture(*opaquePass->GetOutputImageDepth());
	cloudPass->SetTransmittanceLUT(*lutPass->GetTransmittanceLUT(), *lutPass->GetTransmittanceLUTSampler());
	cloudPass->SetNoise(*blueNoise);
	cloudPass->SetCloudMask(*cloudEditor->GetCloudMap());
	cloudPass->Init(ctx, samplerManager, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	cloudTRPass = std::make_unique<CloudTRPass>(*cloudPass);
	cloudTRPass->Init(ctx, samplerManager, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	hillairePass = std::make_unique<HillairePass>(*lowDepthPass, *lutPass, *cloudTRPass);
	hillairePass->SetOpaqueTexture(*opaquePass->GetOutputImage());
	hillairePass->SetOpaqueDepthTexture(*opaquePass->GetOutputImageDepth());
	hillairePass->SetShadowMap(*shadowPass->GetShadowMap());
	hillairePass->SetShadowSampler(*shadowPass->GetShadowSampler());
	hillairePass->SetImageSize(width, height);
	hillairePass->Init(ctx, samplerManager, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	currentAtmospherePass = atmospherePass.get();

	postProcessPass = std::make_unique<PostProcessPass>(*currentAtmospherePass->GetOutputImage());
	postProcessPass->Init(ctx, samplerManager, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	blitPass = std::make_unique<BlitPass>();
	blitPass->Init(ctx, samplerManager, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	allPasses = { shadowPass.get(), opaquePass.get(), lowDepthPass.get(), lutPass.get(), atmospherePass.get(), hillairePass.get(), cloudPass.get(), cloudTRPass.get(), postProcessPass.get(), blitPass.get()};
	activePasses = { shadowPass.get(), opaquePass.get(), atmospherePass.get(), postProcessPass.get(), blitPass.get() };

	UpdateSun();
}

auto BasisScene::GetActivePassList() -> std::vector<APass*>&
{
	if (cloudEditor->IsEnable())
	{
		activePasses2 = activePasses;
		activePasses2.insert(activePasses2.begin(), cloudEditor->GetPass());
		return activePasses2;
	}
	return activePasses;
}

void BasisScene::BeginBuildCommandBuffer()
{
	for (const Drawable& drawable : drawables)
	{
		opaquePass->PushDrawable(drawable);
		shadowPass->PushDrawable(drawable);
	}
}

void BasisScene::DrawDebugGUI()
{
	FPSCamera& camera = static_cast<FPSCamera&>(*GetCamera());

	DrawOverlay();

	ImGui::SetNextWindowSize(ImVec2{ 500.f, 500.f }, ImGuiCond_::ImGuiCond_Appearing);
	if (ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_::ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::Button("Scene"))
				menu = 0;
			if (ImGui::Button("Atmosphere"))
				menu = 1;
			if (ImGui::Button("Camera"))
				menu = 2;
			if (ImGui::Button("Quality"))
				menu = 3;
			if (ImGui::Button("Cloud"))
				menu = 4;
			if (ImGui::Button("Preset"))
				menu = 5;
			ImGui::EndMenuBar();
		}

		ImGui::BeginDisabled(rmseMeasurement.IsRunning());
		if (menu == 0)
		{
			if (ImGui::Button("Mountain"))
			{
				drawables.clear();
				CreateDrawables();
			}
			if (ImGui::Button("City"))
			{
				drawables.clear();
				CreateCityDrawables();
			}
		}
		else if (menu == 1) // Atmosphere
		{
			if (!rmseMeasurement.IsRunning() && ImGui::Button("Change Atmosphere model"))
			{
				changeAtmosphereReq.bValid = true;
				changeAtmosphereReq.bHillaire = currentAtmospherePass == atmospherePass.get();
			}

			if (currentAtmospherePass == atmospherePass.get())
			{
				AtmospherePass* const pass = static_cast<AtmospherePass*>(currentAtmospherePass);
				AtmospherePass::Setting setting = pass->GetSetting();
				int atmoRadiusKM = static_cast<int>(setting.radius / 1000.f);
				if (ImGui::SliderInt("Atmosphere Radius(km)", &atmoRadiusKM, 6370, 10000))
				{
					setting.radius = atmoRadiusKM * 1000.f;
					pass->SetSetting(setting);
				}
				if (ImGui::SliderFloat("Mie Coefficient", &setting.mieCoefficient, 0.0, 100.0))
					pass->SetSetting(setting);
				if (ImGui::SliderFloat("Mie G", &setting.mieG, 0.0, 1.0))
					pass->SetSetting(setting);
			}
			else
			{
				HillairePass* const pass = static_cast<HillairePass*>(currentAtmospherePass);
				LUTPass::GlobalSetting& setting = lutPass->globalSetting;
				int atmoRadiusKM = static_cast<int>(lutPass->globalSetting.atmosphereRadius / 1000.f);
				if (ImGui::SliderInt("Atmosphere Radius(km)", &atmoRadiusKM, 6370, 10000))
				{
					setting.atmosphereRadius = atmoRadiusKM * 1000.f;
					lutPass->UpdateLUTFlags(LUTPass::LUTType::Transmittance);
				}
				if (ImGui::SliderFloat("Mie Coefficient", &setting.mieCoefficient, 0.0, 100.0))
					lutPass->UpdateLUTFlags(LUTPass::LUTType::Transmittance);
				if (ImGui::SliderFloat("Mie G", &setting.mieG, 0.0, 1.0))
					lutPass->UpdateLUTFlags(LUTPass::LUTType::Transmittance);
			}

			if (ImGui::SliderFloat("Sun Illuminance", &sun.w, 0.f, 1000.f))
			{
				UpdateSun();
			}
			static float angle = 0.0f;
			if (ImGui::SliderFloat("Sun Direction", &angle, 0.f, 360.f))
			{
				glm::quat q = glm::quat{ glm::vec3(0.f, 0.f, glm::radians(angle)) };
				glm::vec3 sunDir = glm::normalize(q * glm::normalize(glm::vec3{ -1.f, 0.f, -1.f }));
				sun = glm::vec4(sunDir, sun.w);

				UpdateSun();
			}
		}
		else if (menu == 2) // Camera
		{
			ImGui::Text("Cam pos");
			float pos[] = { camera.GetPos().x, camera.GetPos().y, camera.GetPos().z };
			if (ImGui::InputFloat3("##camPos", pos, "%.3f", ImGuiInputTextFlags_::ImGuiInputTextFlags_EnterReturnsTrue))
			{
				camera.SetPos({ pos[0], pos[1], pos[2] });
				camera.UpdateMatrix();
				UpdateCameraData();
			}

			ImGui::Text("Exposure");
			float exposure = postProcessPass->GetExposure();
			if (ImGui::SliderFloat("##exposure", &exposure, 0.f, 1.f))
				postProcessPass->SetExposure(exposure);
		}
		else if (menu == 3) // Quality
		{
			if (currentAtmospherePass == atmospherePass.get())
			{
				AtmospherePass::Setting setting = atmospherePass->GetSetting();
				ImGui::Text("View Steps");
				if (ImGui::SliderInt("##viewSteps", &setting.steps.x, 1, 256))
					atmospherePass->SetSetting(setting);
				ImGui::Text("Sky-View Steps");
				if (ImGui::SliderInt("##skyViewSteps", &setting.steps.y, 1, 256))
					atmospherePass->SetSetting(setting);
			}
			else
			{
				HillairePass::Setting setting = hillairePass->GetSetting();
				ImGui::Separator();
				if (ImGui::CollapsingHeader("Transmittance LUT"))
				{
					ImGui::Text("Steps");
					if (ImGui::SliderInt("##TransmittanceLUTSteps", reinterpret_cast<int*>(&lutPass->globalSetting.transmittanceLUTSteps), 1, 64))
						lutPass->UpdateLUTFlags(LUTPass::LUTType::Transmittance);
				}
				if (ImGui::CollapsingHeader("MultipleScattering LUT"))
				{
					ImGui::Text("Steps");
					if (ImGui::SliderInt("##MSLUTSteps", reinterpret_cast<int*>(&lutPass->globalSetting.msLUTSteps), 1, 64))
						lutPass->UpdateLUTFlags(LUTPass::LUTType::MultipleScattering);
				}
				
				if (ImGui::CollapsingHeader("Sky-View LUT"))
				{
					ImGui::Text("Steps");
					if (ImGui::SliderInt("##SkyViewLUTSteps", reinterpret_cast<int*>(&lutPass->globalSetting.skyViewLUTSteps), 1, 64))
						lutPass->UpdateLUTFlags(LUTPass::LUTType::SkyView);
					ImGui::Text("Width / Height");
					int size[2] = { lutPass->GetSkyViewLUT()->GetInfo().extent.width, lutPass->GetSkyViewLUT()->GetInfo().extent.height };
					if (ImGui::InputInt2("##SkyViewSize", size, ImGuiInputTextFlags_::ImGuiInputTextFlags_EnterReturnsTrue))
					{
						size[0] = std::clamp(size[0], 1, 4096);
						size[1] = std::clamp(size[1], 1, 4096);
						ImageReCreateRequest request{};
						request.img = lutPass->GetSkyViewLUT();
						request.width = size[0];
						request.height = size[1];
						imgRecreateRequests.insert_or_assign(lutPass->GetSkyViewLUT(), request);
						lutPass->UpdateLUTFlags(LUTPass::LUTType::SkyView);
					}
				}

				if (ImGui::CollapsingHeader("AerialPerspective LUT"))
				{
					bool bActive = setting.modeFlags & 0b01;
					if (ImGui::Checkbox("Toggle##AP", &bActive))
					{
						setting.modeFlags ^= 0b01;
						lutPass->TogglePass(LUTPass::LUTType::AerialPerspective);

						hillairePass->SetSetting(setting);
					}
					ImGui::Text("Steps");
					if (ImGui::SliderInt("##AerialPerspectiveLUTStep", reinterpret_cast<int*>(&lutPass->globalSetting.aerialPerspectiveLUTSteps), 1, 16))
						lutPass->UpdateLUTFlags(LUTPass::LUTType::AerialPerspective);
					if (ImGui::SliderFloat("Distance Factor", &lutPass->globalSetting.apDistanceFactor, 0.1f, 6.4f))
					{
						lutPass->UpdateLUTFlags(LUTPass::LUTType::AerialPerspective);
						if (currentAtmospherePass == hillairePass.get())
						{
							HillairePass::Setting setting = hillairePass->GetSetting();
							setting.apFactor = lutPass->globalSetting.apDistanceFactor;
							hillairePass->SetSetting(setting);
						}
					}
				}

				if (ImGui::CollapsingHeader("Volumetric Shadow"))
				{
					bool bActive = setting.modeFlags & 0b10;
					if (ImGui::Checkbox("Toggle##VolumetricShadow", &bActive))
					{
						setting.modeFlags ^= 0b10;
						lutPass->TogglePass(LUTPass::LUTType::AerialShadow);
						hillairePass->SetSetting(setting);
					}

					ImGui::Text("Steps");
					if (ImGui::SliderInt("##AerialShadowStep", reinterpret_cast<int*>(&lutPass->globalSetting.aerialShadowSteps), 1, 128))
						lutPass->UpdateLUTFlags(LUTPass::LUTType::AerialPerspective);
					ImGui::Text("Width / Height");
					int size[2] = { lutPass->GetAerialShadowLUT()->GetInfo().extent.width, lutPass->GetAerialShadowLUT()->GetInfo().extent.height };
					if (ImGui::InputInt2("##VolumetricShadowSize", size, ImGuiInputTextFlags_::ImGuiInputTextFlags_EnterReturnsTrue))
					{
						size[0] = std::clamp(size[0], 1, 4096);
						size[1] = std::clamp(size[1], 1, 4096);
						ImageReCreateRequest request{};
						request.img = lutPass->GetAerialShadowLUT();
						request.width = size[0];
						request.height = size[1];
						imgRecreateRequests.insert_or_assign(lutPass->GetAerialShadowLUT(), request);
						lutPass->UpdateLUTFlags(LUTPass::LUTType::AerialShadow);
					}
				}
			}

			ImGui::Separator();
			if (ImGui::Button("Measure"))
			{
				AtmospherePass::Setting atmoSetting = atmospherePass->GetSetting();
				HillairePass::Setting hillSetting = hillairePass->GetSetting();
				if (currentAtmospherePass == atmospherePass.get())
				{
					hillSetting.radius = atmoSetting.radius;
					hillairePass->SetSetting(hillSetting);
				}
				else
				{
					atmoSetting.radius = atmoSetting.radius;
					atmospherePass->SetSetting(atmoSetting);
				}
				rmseMeasurement.Start(currentAtmospherePass == hillairePass.get());
			}

			if (rmseMeasurement.IsRunning())
				ImGui::Text("RMSE: %s", rmseMeasurement.GetStatus());
			if (const auto& result = rmseMeasurement.GetResult())
			{
				ImGui::Text("RGB RMSE: %.8f", result->rmse);
				ImGui::Text("Channel RMSE: %.8f, %.8f, %.8f",
					result->channelRMSE.r, result->channelRMSE.g, result->channelRMSE.b);
				ImGui::Text("Pixels: %zu", result->pixelCount);
			}
			if (!rmseMeasurement.GetError().empty())
				ImGui::TextWrapped("RMSE error: %s", rmseMeasurement.GetError().c_str());
		}
		if (menu == 4)
		{
			if (ImGui::Checkbox("Enable", &bCloudEnable))
			{
				changeAtmosphereReq.bValid = true;
				changeAtmosphereReq.bHillaire = currentAtmospherePass == hillairePass.get();

				HillairePass::Setting setting = hillairePass->GetSetting();
				if (bCloudEnable)
					setting.modeFlags |= 0b0100;
				else
					setting.modeFlags &= 0b1011;
				hillairePass->SetSetting(setting);
			}
			if (ImGui::Button("Editor"))
			{
				cloudEditor->SetEnable(!cloudEditor->IsEnable());
			}
			if (cloudEditor->IsEnable())
			{
				ImGui::SliderInt("Brush Radius", reinterpret_cast<int*>(&cloudEditor->setting.brushRadius), 1, 100);
				ImGui::Separator();
			}
			CloudPass::Setting setting = cloudPass->GetSetting();
			if (ImGui::SliderInt("Steps", reinterpret_cast<int*>(&setting.steps), 1, 128))
			{
				setting.steps = std::max(static_cast<int>(setting.steps), 1);
				cloudPass->SetSetting(setting);
			}
			if (ImGui::SliderInt("LightView Steps", reinterpret_cast<int*>(&setting.lightViewSteps), 1, 128))
			{
				setting.lightViewSteps = std::max(static_cast<int>(setting.lightViewSteps), 1);
				cloudPass->SetSetting(setting);
			}
			bool bLightViewLimit = setting.modeFlags & CloudPass::ModeFlag::LightViewDistanceLimit;
			if (ImGui::Checkbox("LightViewDistance Limit", &bLightViewLimit))
			{
				if (bLightViewLimit)
					setting.modeFlags |= CloudPass::ModeFlag::LightViewDistanceLimit;
				else
					setting.modeFlags ^= CloudPass::ModeFlag::LightViewDistanceLimit;
				cloudPass->SetSetting(setting);
			}

			if (ImGui::SliderFloat("Tiling", &setting.tiling, 10000.0f, 100000.f))
				cloudPass->SetSetting(setting);
			if (ImGui::SliderFloat("Tiling2", &setting.tiling2, 1000.f, 100000.f))
				cloudPass->SetSetting(setting);
			if (ImGui::SliderFloat("Extinction Coefficient", &setting.extinctionCoefficient, 1.f, 500.f))
				cloudPass->SetSetting(setting);
			if (ImGui::SliderFloat("Coverage", &setting.coverage, 0.0f, 1.0f, "%.2f"))
				cloudPass->SetSetting(setting);
			if (ImGui::SliderFloat("Powder Strength", &setting.powderStrength, 0.f, 1.f))
				cloudPass->SetSetting(setting);
			if (ImGui::SliderFloat("Anvil bias", &setting.anvilBias, 0.f, 1.f))
				cloudPass->SetSetting(setting);
			if (ImGui::SliderFloat("Dark Height", &setting.darkHeight, 0.f, 1.f))
				cloudPass->SetSetting(setting);
			if (ImGui::SliderFloat("Dark Strength", &setting.darkStrength, 0.f, 4.f))
				cloudPass->SetSetting(setting);
			if (ImGui::SliderFloat("Density Probability Min", &setting.densityPMin, 0.f, 4.f))
				cloudPass->SetSetting(setting);
			if (ImGui::SliderFloat("Density Probability Factor", &setting.densityPFactor, 0.f, 4.f))
				cloudPass->SetSetting(setting);
			float windVel[2] = { setting.windVelKmh.x, setting.windVelKmh.y };
			if (ImGui::SliderFloat2("Wind (km/h)", windVel, 0.f, 100.f))
			{
				setting.windVelKmh = { windVel[0], windVel[1] };
				cloudPass->SetSetting(setting);
			}

			CloudTRPass::Setting trSetting = cloudTRPass->GetSetting();
			if (ImGui::SliderFloat("History Weight", &trSetting.historyWeight, 0.0f, 1.0f, "%.2f"))
				cloudTRPass->SetSetting(trSetting);
		}
		if (menu == 5)
		{
			DrawPresetGUI();
		}
		ImGui::EndDisabled();
	}
	ImGui::End();
}

void BasisScene::DrawOverlay()
{
	FPSCamera& camera = static_cast<FPSCamera&>(*GetCamera());

	ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_::ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_::ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_::ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_::ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_::ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_::ImGuiWindowFlags_NoInputs;
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
	ImGui::SetNextWindowSize(ImVec2{ 300.f, 0.f }, ImGuiCond_::ImGuiCond_Always);
	if (ImGui::Begin("Overlay", nullptr, windowFlags))
	{
		const glm::vec3& pos = camera.GetPos();
		const glm::vec3& to = camera.GetTo();
		const bool hillaire = currentAtmospherePass == hillairePass.get();
		if (!hillaire)
			ImGui::Text("Atmosphere model: default");
		else
			ImGui::Text("Atmosphere model: hillaire");
		ImGui::Text(std::format("pos: {:.2f}, {:.2f}, {:.2f}", pos.x, pos.y, pos.z).c_str());
		ImGui::Text(std::format("to: {:.2f}, {:.2f}, {:.2f}", to.x, to.y, to.z).c_str());

		auto renderPassElapsedTextFn =
			[](const char* passName, const CircularQueue<double, 10>& queue) -> double
			{
				double sum = 0;
				for (int i = 0; i < queue.Size(); ++i)
					sum += queue[i];
				sum /= queue.MaxSize();
				ImGui::Text(std::format("{}: {:.3}ms", passName, sum).c_str());
				return sum;
			};
		renderPassElapsedTextFn("ShadowPass", shadowPassElapsed);
		renderPassElapsedTextFn("OpaquePass", opaquePassElapsed);
		if (hillaire)
		{
			ImGui::Separator();
			renderPassElapsedTextFn("lowDepthPass", lowDepthPassElapsed);
			const double transmittance = renderPassElapsedTextFn("Transmittance", transmittanceLUTPassElapsed);
			const double ms = renderPassElapsedTextFn("MultipleScattering", msLUTPassElapsed);
			const double skyView = renderPassElapsedTextFn("SkyView", skyViewLUTPassElapsed);
			const double aerialPerspective = renderPassElapsedTextFn("AerialPerspective", aerialPerspectiveLUTPassElapsed);
			const double aerialShadow = renderPassElapsedTextFn("AerialShadow", aerialShadowLUTPassElapsed);
			const double LUTSum = transmittance + skyView + aerialPerspective + aerialShadow;
			ImGui::Text(std::format("LUTPass Sum: {:.3}ms", LUTSum).c_str());
			const double atmosphere = renderPassElapsedTextFn("AtmospherePass", atmospherePassElapsed);
			ImGui::Text(std::format("LUT + AtmospherePass: {:.3}ms", LUTSum + atmosphere).c_str());
			ImGui::Separator();
		}
		else
			renderPassElapsedTextFn("AtmospherePass", atmospherePassElapsed);
		if (bCloudEnable)
			renderPassElapsedTextFn("CloudPass", cloudPassElapsed);
		renderPassElapsedTextFn("PostProcessPass", postProcessPassElapsed);
		ImGui::End();
	}
}

void BasisScene::DrawPresetGUI()
{
	FPSCamera& camera = static_cast<FPSCamera&>(*GetCamera());

	static std::string presetName;
	ImGui::InputText("name", &presetName);
	if (ImGui::Button("Make Preset"))
	{
		const CloudPass::Setting& cloudSetting = cloudPass->GetSetting();

		Preset preset{};
		preset.camPos = camera.GetPos();
		preset.camQuat = camera.GetQuat();
		preset.sun = sun;
		preset.cloudTile = cloudSetting.tiling;
		preset.cloudEC = cloudSetting.extinctionCoefficient;
		preset.cloudCoverage = cloudSetting.coverage;
		preset.cloudDarkHeight = cloudSetting.darkHeight;
		preset.windVel = cloudSetting.windVelKmh;
		preset.cloudAnvil = cloudSetting.anvilBias;
		presetManager.AddPreset(presetName, preset);
		presetName.clear();
	}
	ImGui::Separator();

	for (const auto& presetInfo : presetManager.GetPresets())
	{
		ImGui::Text(presetInfo.name.c_str());
		ImGui::SameLine();
		if (ImGui::Button(std::format("Load##{}", presetInfo.name).c_str()))
		{
			camera.SetPos(presetInfo.preset.camPos);
			camera.SetQuat(presetInfo.preset.camQuat);
			camera.UpdateMatrix();

			sun = presetInfo.preset.sun;
			CloudPass::Setting cloudSetting = cloudPass->GetSetting();
			cloudSetting.tiling = presetInfo.preset.cloudTile;
			cloudSetting.extinctionCoefficient = presetInfo.preset.cloudEC;
			cloudSetting.coverage = presetInfo.preset.cloudCoverage;
			cloudSetting.darkHeight = presetInfo.preset.cloudDarkHeight;
			cloudSetting.windVelKmh = presetInfo.preset.windVel;
			cloudSetting.anvilBias = presetInfo.preset.cloudAnvil;
			cloudPass->SetSetting(cloudSetting);

			UpdateSun();
			UpdateCameraData();
		}
		ImGui::SameLine();
		if (ImGui::Button(std::format("Delete##{}", presetInfo.name).c_str()))
			presetManager.DeletePreset(presetInfo.name);
	}

	ImGui::Separator();
	if (ImGui::Button("Export Presets"))
		presetManager.ExportPresets("presets.json");
	if (ImGui::Button("Load Presets"))
		presetManager.LoadPresets("presets.json");
}

void BasisScene::SetAtmosphereModel(bool useHillaire)
{
	cloudTRPass->InvalidateHistory();
	AtmosphereBasePass* requestedPass = useHillaire ? static_cast<AtmosphereBasePass*>(hillairePass.get()) : atmospherePass.get();

	counter = 0;
	atmospherePassElapsed.Clear();
	currentAtmospherePass = requestedPass;

	if (useHillaire)
	{
		SH_INFO("Change to Hillaire");
		if (bCloudEnable)
			activePasses = { shadowPass.get(), opaquePass.get(), lowDepthPass.get(), lutPass.get(), cloudPass.get(), cloudTRPass.get(), hillairePass.get(), postProcessPass.get(), blitPass.get()};
		else
			activePasses = { shadowPass.get(), opaquePass.get(), lowDepthPass.get(), lutPass.get(), hillairePass.get(), postProcessPass.get(), blitPass.get() };
		postProcessPass->SetOutputImage(*hillairePass->GetOutputImage());
	}
	else
	{
		SH_INFO("Change to default");
		activePasses = { shadowPass.get(), opaquePass.get(), atmospherePass.get(), postProcessPass.get(), blitPass.get() };
		postProcessPass->SetOutputImage(*atmospherePass->GetOutputImage());
	}
	UpdateSun();
}

void BasisScene::CreateDrawables()
{
	glm::mat4 rootMatrix = glm::translate(glm::mat4{ 1.f }, glm::vec3{ 0.f, 0.f, 0.f });
	rootMatrix = glm::scale(rootMatrix, glm::vec3{ 10.f, 10.f, 10.f });

	struct BFSInfo
	{
		GLBLoader::Node* node;
		glm::mat4 parentModelMatrix;
	};
	std::queue<BFSInfo> bfs;
	bfs.push({ &mountain.model.nodes[0], rootMatrix });
	while (!bfs.empty())
	{
		auto [nodePtr, parentModelMatrix] = bfs.front();
		bfs.pop();

		const glm::mat4 modelMatrix = parentModelMatrix * nodePtr->modelMatrix;
		if (nodePtr->meshPtr != nullptr)
		{
			Drawable& drawable = drawables.emplace_back();
			drawable.modelMatrix = modelMatrix;
			drawable.mesh = nodePtr->meshPtr.get();
			drawable.mat = mountain.material.get();
		}

		for (int idx : nodePtr->childrenIdxs)
		{
			GLBLoader::Node& child = mountain.model.nodes[idx];
			bfs.push({ &child, modelMatrix });
		}
	}
}

void BasisScene::CreateCityDrawables()
{
	glm::mat4 rootMatrix = glm::translate(glm::mat4{ 1.f }, glm::vec3{ 0.f, 0.f, 0.f });
	rootMatrix = glm::scale(rootMatrix, glm::vec3{ 10.f, 10.f, 10.f });

	struct BFSInfo
	{
		GLBLoader::Node* node;
		glm::mat4 parentModelMatrix;
	};
	std::queue<BFSInfo> bfs;
	bfs.push({ &city.model.nodes[0], rootMatrix });
	while (!bfs.empty())
	{
		auto [nodePtr, parentModelMatrix] = bfs.front();
		bfs.pop();

		const glm::mat4 modelMatrix = parentModelMatrix * nodePtr->modelMatrix;
		if (nodePtr->meshPtr != nullptr)
		{
			Drawable& drawable = drawables.emplace_back();
			drawable.modelMatrix = modelMatrix;
			drawable.mesh = nodePtr->meshPtr.get();
			if (nodePtr->textureIdx >= 0)
				drawable.mat = city.materials[nodePtr->textureIdx].get();
		}

		for (int idx : nodePtr->childrenIdxs)
		{
			GLBLoader::Node& child = city.model.nodes[idx];
			bfs.push({ &child, modelMatrix });
		}
	}
}

void BasisScene::ControlCamera(double dt)
{
	if (ImGui::GetIO().WantTextInput)
		return;
	FPSCamera& camera = static_cast<FPSCamera&>(*GetCamera());
	if (Input::IsKeyDown(Event::KeyType::Up))
	{
		camera.AddPitch(60.0 * dt);
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::Down))
	{
		camera.AddPitch(-60.0 * dt);
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::Left))
	{
		camera.AddYaw(120.0 * dt);
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::Right))
	{
		camera.AddYaw(-120.0 * dt);
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::W))
	{
		const glm::vec3 forward = glm::normalize(camera.GetTo() - camera.GetPos()) * 500.f * static_cast<float>(dt);
		camera.SetPos(camera.GetPos() + forward);
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::S))
	{
		const glm::vec3 forward = glm::normalize(camera.GetTo() - camera.GetPos()) * 500.f * static_cast<float>(dt);
		camera.SetPos(camera.GetPos() - forward);
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::D))
	{
		const glm::vec3 forward = glm::normalize(camera.GetTo() - camera.GetPos());
		const glm::vec3 right = glm::cross(forward, camera.GetUp()) * 500.f * static_cast<float>(dt);
		camera.SetPos(camera.GetPos() + right);
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::A))
	{
		const glm::vec3 forward = glm::normalize(camera.GetTo() - camera.GetPos());
		const glm::vec3 right = glm::cross(forward, camera.GetUp()) * 500.f * static_cast<float>(dt);
		camera.SetPos(camera.GetPos() - right);
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::Space))
	{
		glm::vec3 pos = camera.GetPos();
		pos.y += 1000.0 * dt;
		camera.SetPos(pos);
		camera.UpdateMatrix();
		lutPass->UpdateLUTFlags(LUTPass::LUTType::SkyView);
	}
	if (Input::IsKeyDown(Event::KeyType::LCtrl))
	{
		glm::vec3 pos = camera.GetPos();
		pos.y -= 1000.0 * dt;
		camera.SetPos(pos);
		camera.UpdateMatrix();
		lutPass->UpdateLUTFlags(LUTPass::LUTType::SkyView);
	}
	UpdateCameraData();
	lutPass->UpdateLUTFlags(LUTPass::LUTType::AerialPerspective); // 카메라 때문에 매번 업데이트 해야함
}

void BasisScene::UpdateSun()
{
	const float length = 10'000.f;
	Camera sunCamera{};
	sunCamera.SetPos({ -sun.x * length, -sun.y * length, -sun.z * length });
	sunCamera.SetTo({ 0.f, 0.f, 0.f });
	sunCamera.SetNear(1000.f);
	sunCamera.SetFar(100'000.f);
	sunCamera.SetOrtho();
	sunCamera.SetWidth(length * 2.f);
	sunCamera.SetHeight(length * 2.f);
	sunCamera.UpdateMatrix();

	const glm::mat4 sunViewProj = sunCamera.GetMatrixProj() * sunCamera.GetMatrixView();;

	mountain.data.sun = sun;
	mountain.data.viewProj = sunViewProj;
	mountain.material->UpdateBindingData(0, mountain.data);
	city.data.sun = sun;
	city.data.viewProj = sunViewProj;
	for (std::unique_ptr<Material>& matPtr : city.materials)
		matPtr->UpdateBindingData(0, city.data);

	AtmospherePass::Setting atmosphere = atmospherePass->GetSetting();
	atmosphere.sun = sun;
	atmosphere.sunViewProj = mountain.data.viewProj;
	atmospherePass->SetSetting(atmosphere);

	HillairePass::Setting hillaireSetting = hillairePass->GetSetting();
	hillaireSetting.sun = sun;
	hillairePass->SetSetting(hillaireSetting);

	lutPass->globalSetting.sun = sun;
	lutPass->globalSetting.sunViewProj = sunViewProj;
	lutPass->UpdateLUTFlags(LUTPass::LUTType::SkyView | LUTPass::LUTType::AerialPerspective);

	shadowPass->SetCamera(sunCamera);

	CloudPass::Setting cloudPassSetting = cloudPass->GetSetting();
	cloudPassSetting.sun = sun;
	cloudPass->SetSetting(cloudPassSetting);
}

auto BasisScene::Preset::Serialize() const -> Json
{
	Json json;
	json["camPos"] = { camPos.x, camPos.y, camPos.z };
	json["camQuat"] = { camQuat.x, camQuat.y, camQuat.z, camQuat.w };
	json["sun"] = { sun.x, sun.y, sun.z, sun.w };
	json["cloud"] = { cloudTile, cloudEC, cloudCoverage, windVel.x, windVel.y, cloudDarkHeight, cloudAnvil };
	return json;
}

void BasisScene::Preset::Deserialize(const Json& json)
{
	if (auto it = json.find("camPos"); it != json.end())
	{
		camPos.x = it.value()[0]; camPos.y = it.value()[1]; camPos.z = it.value()[2];
	}
	if (auto it = json.find("camQuat"); it != json.end())
	{
		camQuat.x = it.value()[0]; camQuat.y = it.value()[1]; camQuat.z = it.value()[2]; camQuat.w = it.value()[3];
	}
	if (auto it = json.find("sun"); it != json.end())
	{
		sun.x = it.value()[0]; sun.y = it.value()[1]; sun.z = it.value()[2]; sun.w = it.value()[3];
	}
	if (auto it = json.find("cloud"); it != json.end())
	{
		const std::size_t size = it.value().size();
		cloudTile = it.value()[0];
		cloudEC = it.value()[1];
		cloudCoverage = it.value()[2];
		windVel.x = it.value()[3];
		windVel.y = it.value()[4];
		if (size >= 6)
			cloudDarkHeight = it.value()[5];
		if (size >= 7)
			cloudAnvil = it.value()[6];
	}
}
