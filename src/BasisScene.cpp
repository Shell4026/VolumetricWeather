#include "BasisScene.h"
#include "FPSCamera.h"

#include "core/Input.h"

#include "render/Material.h"

#include "pass/OpaquePass.h"
#include "pass/AtmospherePass.h"
#include "pass/PostProcessPass.h"

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

	for (APass* pass : activePasses)
		pass->Clear();
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
	ControlCamera(dt);
	DrawDebugGUI();
}

void BasisScene::Render(double dt)
{
	if (counter > 0)
	{
		opaquePassElapsed.Push(opaquePass->GetElapsedTimeMs());
		atmospherePassElapsed.Push(atmospherePass->GetElapsedTimeMs());
		postProcessPassElapsed.Push(postProcessPass->GetElapsedTimeMs());
	}
	AScene::Render(dt);
	++counter;
}

auto BasisScene::CreateSceneCamera() -> std::unique_ptr<Camera>
{
	std::unique_ptr<FPSCamera> camPtr = std::make_unique<FPSCamera>();
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
	VkSamplerCreateInfo samplerCi{};
	samplerCi.sType = VkStructureType::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCi.magFilter = VkFilter::VK_FILTER_LINEAR;
	samplerCi.minFilter = VkFilter::VK_FILTER_LINEAR;
	samplerCi.mipmapMode = VkSamplerMipmapMode::VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCi.addressModeU = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCi.addressModeV = samplerCi.addressModeU;
	samplerCi.addressModeW = samplerCi.addressModeU;
	samplerCi.mipLodBias = 0.0f;
	samplerCi.maxAnisotropy = 1.0f;
	samplerCi.compareOp = VkCompareOp::VK_COMPARE_OP_NEVER;
	samplerCi.minLod = 0.0f;
	samplerCi.maxLod = 1.0f;
	samplerCi.borderColor = VkBorderColor::VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	VK_RESULT_CHECK(vkCreateSampler(ctx.GetDevice(), &samplerCi, nullptr, &sampler));

	// opaqueShader 초기화
	std::vector<VkDescriptorSetLayoutBinding> set1Bindings;
	VkDescriptorSetLayoutBinding& binding0 = set1Bindings.emplace_back();
	binding0.binding = 0;
	binding0.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
	binding0.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding0.descriptorCount = 1;
	VkDescriptorSetLayoutBinding& binding1 = set1Bindings.emplace_back();
	binding1.binding = 1;
	binding1.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
	binding1.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding1.descriptorCount = 1;

	VkPushConstantRange pc{};
	pc.size = sizeof(glm::mat4);
	pc.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;

	opaqueShader.AddSet(0, GetCameraDescriptorSetLayout());
	opaqueShader.AddSet(1, std::move(set1Bindings));
	opaqueShader.Build(ctx.GetDevice(), "shaders/mesh.vert.spv", "shaders/mesh.frag.spv", &pc);

	// 산
	mountain.model = GLBLoader::LoadGLB(ctx, "models/mountain_1.glb");

	mountain.material = std::make_unique<Material>(ctx, opaqueShader);
	mountain.material->AddBinding<Mountain::MaterialData>(0);
	mountain.material->AddBinding(1, mountain.model.textures[0], sampler);
	mountain.material->Build(GetDescriptorPool());

	mountain.data.sun = sun;
	mountain.material->UpdateBindingData(0, mountain.data);

	CreateDrawables();
}

void BasisScene::SetupPass()
{
	opaquePass = std::make_unique<OpaquePass>();
	opaquePass->SetShader(opaqueShader);
	opaquePass->Init(ctx, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	atmospherePass = std::make_unique<AtmospherePass>();
	atmospherePass->SetOpaqueTexture(*opaquePass->GetOutputImage());
	atmospherePass->SetOpaqueDepthTexture(*opaquePass->GetOutputImageDepth());
	atmospherePass->Init(ctx, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	postProcessPass = std::make_unique<PostProcessPass>(*atmospherePass->GetOutputImage());
	postProcessPass->Init(ctx, GetDescriptorPool(), GetCameraDescriptorSetLayout());

	activePasses = { opaquePass.get(), atmospherePass.get(), postProcessPass.get() };
}

void BasisScene::BeginBuildCommandBuffer()
{
	for (const Drawable& drawable : drawables)
		opaquePass->PushDrawable(drawable);
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
		ImGui::Text(std::format("pos: {:.2f}, {:.2f}, {:.2f}", pos.x, pos.y, pos.z).c_str());
		ImGui::Text(std::format("to: {:.2f}, {:.2f}, {:.2f}", to.x, to.y, to.z).c_str());
		double sum = 0;
		for (int i = 0; i < opaquePassElapsed.Size(); ++i)
			sum += opaquePassElapsed[i];
		ImGui::Text(std::format("OpaquePass: {:.2}ms", sum / opaquePassElapsed.MaxSize()).c_str());
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
	if (ImGui::Begin("Debug"))
	{
		AtmospherePass::Atmosphere atmosphere = atmospherePass->GetAtmosphere();
		ImGui::Text("View Steps");
		if (ImGui::SliderInt("##viewSteps", &atmosphere.steps.x, 1, 256))
			atmospherePass->SetAtmosphere(atmosphere);
		ImGui::Text("Sky-View Steps");
		if (ImGui::SliderInt("##skyViewSteps", &atmosphere.steps.y, 1, 256))
			atmospherePass->SetAtmosphere(atmosphere);

		ImGui::Text("Sun illuminance");
		if (ImGui::SliderFloat("##SunIlluminance", &sun.w, 0.f, 1000.f))
		{
			mountain.data.sun = sun;
			mountain.material->UpdateBindingData(0, mountain.data);

			atmosphere.sun = sun;
			atmospherePass->SetAtmosphere(atmosphere);
		}

		ImGui::Text("Sun direction");
		static float angle = 0.0f;
		if (ImGui::SliderFloat("##SunDirection", &angle, 0.f, 360.f))
		{
			glm::quat q = glm::quat{ glm::vec3(0.f, 0.f, glm::radians(angle)) };
			glm::vec3 sunDir = q * glm::normalize(glm::vec3{ -1.f, 0.f, -1.f });
			sun = glm::vec4(sunDir, sun.w);
			
			mountain.data.sun = sun;
			mountain.material->UpdateBindingData(0, mountain.data);

			atmosphere.sun = sun;
			atmospherePass->SetAtmosphere(atmosphere);
		}

		ImGui::Text("Cam pos");
		float pos[] = { camera.GetPos().x, camera.GetPos().y, camera.GetPos().z };
		if (ImGui::InputFloat3("##camPos", pos, "%.3f", ImGuiInputTextFlags_::ImGuiInputTextFlags_EnterReturnsTrue))
		{
			camera.SetPos({ pos[0], pos[1], pos[2] });
			camera.CalcTo();
			camera.UpdateMatrix();
			UpdateCameraData();
		}

		ImGui::Text("Exposure");
		float exposure = postProcessPass->GetExposure();
		if (ImGui::SliderFloat("##exposure", &exposure, 0.f, 1.f))
			postProcessPass->SetExposure(exposure);

		ImGui::End();
	}
}

void BasisScene::CreateDrawables()
{
	struct BFSInfo
	{
		GLBLoader::Node* node;
		glm::mat4 parentModelMatrix;
	};
	std::queue<BFSInfo> bfs;
	bfs.push({ &mountain.model.nodes[0], glm::mat4{1.f} });
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
		camera.AddYaw(-30.0 * dt);
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::D))
	{
		camera.AddYaw(30.0 * dt);
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::Space))
	{
		glm::vec3 pos = camera.GetPos();
		pos.y += 1000.0 * dt;
		camera.SetPos(pos);
		camera.CalcTo();
		camera.UpdateMatrix();
	}
	if (Input::IsKeyDown(Event::KeyType::LCtrl))
	{
		glm::vec3 pos = camera.GetPos();
		pos.y -= 1000.0 * dt;
		camera.SetPos(pos);
		camera.CalcTo();
		camera.UpdateMatrix();
	}
	UpdateCameraData();
}