#include "BasisScene.h"
#include "FPSCamera.h"

#include "core/Input.h"
#include "core/Window.h"

#include "render/Material.h"

#include "pass/ShadowPass.h"
#include "pass/OpaquePass.h"
#include "pass/LUTPass.h"
#include "pass/AtmospherePass.h"
#include "pass/HillairePass.h"
#include "pass/PostProcessPass.h"
#include "pass/BlitPass.h"

#include "imgui/imgui.h"
#include "glm/gtc/quaternion.hpp"

#include <queue>
BasisScene::BasisScene(VulkanContext& ctx, const ImGUI& imgui, Window& window) :
	AScene(ctx, imgui, window)
{
	const glm::quat q = glm::quat{ glm::vec3(0.f, 0.f, glm::radians(0.f)) };
	const glm::vec3 sunDir = q * glm::normalize(glm::vec3{ -1.f, 0.f, -1.f });
	sun = glm::vec4{ sunDir, sun.w };
}

BasisScene::~BasisScene()
{
	Clear();
}

void BasisScene::Clear()
{
	const VkDevice device = ctx.GetDevice();
	vkDeviceWaitIdle(device);

	for (APass* pass : allPasses)
		pass->Clear();
	allPasses.clear();
	activePasses.clear();

	opaqueShader.Clear();

	mountain = Mountain{};

	if (sampler != VK_NULL_HANDLE)
	{
		vkDestroySampler(device, sampler, nullptr);
		sampler = VK_NULL_HANDLE;
	}
	AScene::Clear();
}

void BasisScene::Update(double dt)
{
	if (!rmseMeasurement.IsRunning())
		ControlCamera(dt);
	DrawDebugGUI();
}

void BasisScene::Render(double dt)
{
	if (bChangeAtmosphereModelRequest)
	{
		SetAtmosphereModel(currentAtmospherePass == atmospherePass.get());
		bChangeAtmosphereModelRequest = false;
	}
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
			transmittanceLUTPassElapsed.Push(lutPass->GetElapsedTimeMs());
		atmospherePassElapsed.Push(currentAtmospherePass->GetElapsedTimeMs());
		postProcessPassElapsed.Push(postProcessPass->GetElapsedTimeMs());
	}
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

	CreateDrawables();
}

void BasisScene::SetupPass()
{
	shadowPass = std::make_unique<ShadowPass>();
	shadowPass->Init(ctx, GetDescriptorPool(), VK_NULL_HANDLE);
	mountain.material->UpdateBindingData(2, *shadowPass->GetShadowMap(), shadowPass->GetShadowSampler()->GetSampler());

	opaquePass = std::make_unique<OpaquePass>();
	opaquePass->SetShader(opaqueShader);
	opaquePass->SetImageSize(window.GetWidth(), window.GetHeight());
	opaquePass->Init(ctx, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	lutPass = std::make_unique<LUTPass>();
	lutPass->Init(ctx, GetDescriptorPool(), VK_NULL_HANDLE);

	atmospherePass = std::make_unique<AtmospherePass>();
	atmospherePass->SetOpaqueTexture(*opaquePass->GetOutputImage());
	atmospherePass->SetOpaqueDepthTexture(*opaquePass->GetOutputImageDepth());
	atmospherePass->SetShadowMap(*shadowPass->GetShadowMap());
	atmospherePass->SetShadowSampler(*shadowPass->GetShadowSampler());
	atmospherePass->SetImageSize(window.GetWidth(), window.GetHeight());
	atmospherePass->Init(ctx, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	hillairePass = std::make_unique<HillairePass>();
	hillairePass->SetOpaqueTexture(*opaquePass->GetOutputImage());
	hillairePass->SetOpaqueDepthTexture(*opaquePass->GetOutputImageDepth());
	hillairePass->SetShadowMap(*shadowPass->GetShadowMap());
	hillairePass->SetShadowSampler(*shadowPass->GetShadowSampler());
	hillairePass->SetTransmittanceLUT(*lutPass->GetTransmittanceLUT(), *lutPass->GetTransmittanceLUTSampler());
	hillairePass->SetImageSize(window.GetWidth(), window.GetHeight());
	hillairePass->Init(ctx, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	currentAtmospherePass = atmospherePass.get();

	postProcessPass = std::make_unique<PostProcessPass>(*currentAtmospherePass->GetOutputImage());
	postProcessPass->Init(ctx, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	blitPass = std::make_unique<BlitPass>();
	blitPass->Init(ctx, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	allPasses = { shadowPass.get(), opaquePass.get(), lutPass.get(), atmospherePass.get(), hillairePass.get(), postProcessPass.get(), blitPass.get() };
	activePasses = { shadowPass.get(), opaquePass.get(), atmospherePass.get(), postProcessPass.get(), blitPass.get() };

	UpdateSun();
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
		double sum = 0;
		for (int i = 0; i < shadowPassElapsed.Size(); ++i)
			sum += shadowPassElapsed[i];
		ImGui::Text(std::format("ShadowPass: {:.2}ms", sum / shadowPassElapsed.MaxSize()).c_str());
		sum = 0;
		for (int i = 0; i < opaquePassElapsed.Size(); ++i)
			sum += opaquePassElapsed[i];
		ImGui::Text(std::format("OpaquePass: {:.2}ms", sum / opaquePassElapsed.MaxSize()).c_str());
		if (hillaire)
		{
			sum = 0;
			for (int i = 0; i < transmittanceLUTPassElapsed.Size(); ++i)
				sum += transmittanceLUTPassElapsed[i];
			ImGui::Text(std::format("LUTPass: {:.2}ms", sum / transmittanceLUTPassElapsed.MaxSize()).c_str());
		}
		sum = 0;
		for (int i = 0; i < atmospherePassElapsed.Size(); ++i)
			sum += atmospherePassElapsed[i];
		ImGui::Text(std::format("AtmospherePass: {:.2}ms", sum / atmospherePassElapsed.MaxSize()).c_str());
		sum = 0;
		for (int i = 0; i < postProcessPassElapsed.Size(); ++i)
			sum += postProcessPassElapsed[i];
		ImGui::Text(std::format("PostProcessPass: {:.2}ms", sum / postProcessPassElapsed.MaxSize()).c_str());
	}
	ImGui::End();

	ImGui::SetNextWindowSize(ImVec2{ 300.f, 300.f }, ImGuiCond_::ImGuiCond_Appearing);
	if (ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_::ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::Button("Atmosphere"))
				menu = 0;
			if (ImGui::Button("Camera"))
				menu = 1;
			if (ImGui::Button("Quality"))
				menu = 2;
			ImGui::EndMenuBar();
		}

		AtmospherePass::Atmosphere atmosphere = currentAtmospherePass->GetAtmosphere();
		ImGui::BeginDisabled(rmseMeasurement.IsRunning());
		if (menu == 0) // Atmosphere
		{
			if (!rmseMeasurement.IsRunning() && ImGui::Button("Change Atmosphere model"))
				bChangeAtmosphereModelRequest = true;

			ImGui::Text("Atmosphere radius(km)");
			int atmoRadiusKM = static_cast<int>(atmosphere.radius / 1000.f);
			if (ImGui::SliderInt("##atmosphereRadius", &atmoRadiusKM, 6370, 10000))
			{
				atmosphere.radius = atmoRadiusKM * 1000.f;
				currentAtmospherePass->SetAtmosphere(atmosphere);
			}

			ImGui::Text("Sun illuminance");
			if (ImGui::SliderFloat("##SunIlluminance", &sun.w, 0.f, 1000.f))
			{
				UpdateSun();
			}

			ImGui::Text("Sun direction");
			static float angle = 0.0f;
			if (ImGui::SliderFloat("##SunDirection", &angle, 0.f, 360.f))
			{
				glm::quat q = glm::quat{ glm::vec3(0.f, 0.f, glm::radians(angle)) };
				glm::vec3 sunDir = q * glm::normalize(glm::vec3{ -1.f, 0.f, -1.f });
				sun = glm::vec4(sunDir, sun.w);

				UpdateSun();
			}
		}
		else if (menu == 1) // Camera
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
		else if (menu == 2) // Quality
		{
			ImGui::Text("View Steps");
			if (ImGui::SliderInt("##viewSteps", &atmosphere.steps.x, 1, 256))
				currentAtmospherePass->SetAtmosphere(atmosphere);
			ImGui::Text("Sky-View Steps");
			if (ImGui::SliderInt("##skyViewSteps", &atmosphere.steps.y, 1, 256))
				currentAtmospherePass->SetAtmosphere(atmosphere);

			if (ImGui::Button("Measure"))
			{
				const AtmospherePass::Atmosphere settings = currentAtmospherePass->GetAtmosphere();
				atmospherePass->SetAtmosphere(settings);
				hillairePass->SetAtmosphere(settings);
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
		ImGui::EndDisabled();
		ImGui::End();
	}
}

void BasisScene::SetAtmosphereModel(bool useHillaire)
{
	AtmospherePass* requestedPass = useHillaire
		? static_cast<AtmospherePass*>(hillairePass.get())
		: atmospherePass.get();
	if (currentAtmospherePass == requestedPass)
		return;

	counter = 0;
	atmospherePassElapsed.Clear();
	currentAtmospherePass = requestedPass;

	if (useHillaire)
	{
		SH_INFO("Change to Hillaire");
		activePasses = { shadowPass.get(), opaquePass.get(), lutPass.get(), hillairePass.get(), postProcessPass.get(), blitPass.get() };
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
	glm::mat4 rootMatrix = glm::translate(glm::mat4{ 1.f }, glm::vec3{ 0.f, -700.f, 0.f });
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
	return;
}

void BasisScene::ControlCamera(double dt)
{
	FPSCamera& camera = static_cast<FPSCamera&>(*GetCamera());
	if (Input::IsKeyDown(Event::KeyType::W))
	{
		camera.AddPitch(30.0 * dt);
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::S))
	{
		camera.AddPitch(-30.0 * dt);
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::A))
	{
		camera.AddYaw(30.0 * dt);
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::D))
	{
		camera.AddYaw(-30.0 * dt);
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::Space))
	{
		glm::vec3 pos = camera.GetPos();
		pos.y += 1000.0 * dt;
		camera.SetPos(pos);
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::LCtrl))
	{
		glm::vec3 pos = camera.GetPos();
		pos.y -= 1000.0 * dt;
		camera.SetPos(pos);
		camera.UpdateMatrix();
	}
	UpdateCameraData();
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

	AtmospherePass::Atmosphere atmosphere = currentAtmospherePass->GetAtmosphere();
	mountain.data.sun = sun;
	mountain.data.viewProj = sunCamera.GetMatrixProj() * sunCamera.GetMatrixView();
	mountain.material->UpdateBindingData(0, mountain.data);

	atmosphere.sun = sun;
	atmosphere.sunViewProj = mountain.data.viewProj;
	currentAtmospherePass->SetAtmosphere(atmosphere);

	shadowPass->SetCamera(sunCamera);
}
