#include "CloudEditor.h"
#include "Scene.h"
#include "Camera.h"

#include "render/VulkanImage.h"

#include "pass/CloudPaintPass.h"

#include <imgui/imgui.h>

#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif
auto RaySphereIntersect(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& sphereOrigin, float sphereRadius, float& near, float& far) -> bool
{
	const glm::vec3 rs = sphereOrigin - rayOrigin;
	float a2 = dot(rs, rs);
	float b = dot(rs, rayDir);
	float c2 = a2 - b * b;
	float r2 = sphereRadius * sphereRadius;
	if (c2 > r2)
		return false;

	float d = sqrt(r2 - c2); // 구 내부 ray길이
	near = b - d;
	far = b + d;

	return far >= 0.0;
}

CloudEditor::CloudEditor(AScene& scene) :
	scene(scene)
{
	PrepareResource();

	paintPass = std::make_unique<CloudPaintPass>();
	paintPass->SetCanvas(*coverageMap);
	paintPass->Init(scene.ctx, scene.samplerManager, scene.GetDescriptorPool(), scene.GetCameraDescriptorSetLayout());
}

CloudEditor::~CloudEditor()
{
	Clear();
}

void CloudEditor::Clear()
{
	coverageMap.reset();
	paintPass.reset();
}

void CloudEditor::Update()
{
	if (!bEnable)
		return;

	const ImGuiIO& io = ImGui::GetIO();
	if (io.MouseDown[0])
	{
		paintPass->pc.brushUV = GetUV(io.MousePos.x, io.MousePos.y);
		paintPass->pc.radius = 2;
		SH_INFO_FORMAT("mousedown, uv: {}, {}", paintPass->pc.brushUV.x, paintPass->pc.brushUV.y);
	}
}

void CloudEditor::PrepareResource()
{
	VkImageCreateInfo ci = VulkanImage::GetCreateInfo();
	ci.extent = { 2048, 2048, 1 };
	ci.format = VkFormat::VK_FORMAT_R8_UNORM;
	ci.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	coverageMap = std::make_unique<VulkanImage>(scene.ctx, ci, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	const std::vector<uint8_t> data(2048 * 2048, 0);
	coverageMap->SetData(data.data(), data.size());
}

auto CloudEditor::GetUV(uint32_t mouseX, uint32_t mouseY) const -> glm::vec2
{
	Camera* const cameraPtr = scene.GetCamera();
	cameraPtr->UpdateMatrix();
	const float width = cameraPtr->GetWidth();
	const float height = cameraPtr->GetHeight();

	const glm::vec3 ndc = { (mouseX / width) * 2.f - 1.f, -((mouseY / height) * 2.f - 1.f), 1.f };
	const glm::vec4 viewPos = cameraPtr->GetMatrixInverseProj() * glm::vec4{ ndc, 1.f };
	const glm::vec3 viewDir = glm::normalize(glm::vec3{ viewPos / viewPos.w });
	const glm::vec3 worldDir = glm::normalize(cameraPtr->GetMatrixInverseView() * glm::vec4{ viewDir, 0.f });

	const glm::vec3 planetCenter{ 0.f, -groundRadius, 0.f };
	const glm::vec3 up = glm::normalize(cameraPtr->GetPos() - planetCenter);
	float near = 0.f;
	float far = 0.f;
	if (RaySphereIntersect(cameraPtr->GetPos(), worldDir, planetCenter, groundRadius + cloudHeight, near, far))
	{
		const glm::vec3 worldPos = cameraPtr->GetPos() + worldDir * far;
		SH_INFO_FORMAT("x: {}, y: {}, z: {}", worldPos.x, worldPos.y, worldPos.z);
		const float worldSizeM = worldSizeKm * 1000.f;
		return glm::vec2{ worldPos.x / worldSizeM + 0.5f, worldPos.z / worldSizeM + 0.5f };
	}
	return glm::vec2{ 0.f, 0.f };
}
