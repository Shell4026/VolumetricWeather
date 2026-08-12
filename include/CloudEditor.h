#pragma once

#include <glm/glm.hpp>

#include <vector>
#include <memory>
class AScene;
class VulkanImage;
class CloudPaintPass;
class CloudEditor
{
public:
	struct Setting
	{
		uint32_t brushRadius = 10;
	} setting;
public:
	CloudEditor(AScene& scene);
	~CloudEditor();

	void Clear();

	void Update();

	void SetPlanetRadius(float groundRadius) { this->groundRadius = groundRadius; }
	void SetEnable(bool b) { bEnable = b; }
	auto IsEnable() const -> bool { return bEnable; }
	auto GetPass() const -> CloudPaintPass* { return paintPass.get(); }
	auto GetCloudMap() const -> VulkanImage* { return coverageMap.get(); }
private:
	void PrepareResource();
	auto GetUV(uint32_t mouseX, uint32_t mouseY) const -> glm::vec2;
private:
	AScene& scene;
	std::unique_ptr<VulkanImage> coverageMap;
	std::unique_ptr<CloudPaintPass> paintPass;

	float groundRadius = 6'360'000.f;
	float cloudHeight = 2'000;
	float worldSizeKm = 128;
	bool bEnable = false;
};