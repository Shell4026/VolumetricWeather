#include "Weather/ArtistGUI.h"

#include "core/Logger.h"

#include <imgui/imgui.h>
ArtistGUI::ArtistGUI(ArtistSetting& setting) :
	setting(setting)
{
}

void ArtistGUI::RenderGUI()
{
	dirtyFlags = 0;
	bool bPlanetUpdate = false;
	bool bSunUpdate = false;
	bool bCloudUpdate = false;
	if (ImGui::BeginChild("ArtistGUIChild", ImVec2{ 0, 0 }, ImGuiChildFlags_::ImGuiChildFlags_Borders, ImGuiWindowFlags_::ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::Button("Planet"))
				menu = menu == 0 ? -1 : 0;
			if (ImGui::Button("Cloud"))
				menu = menu == 1 ? -1 : 1;
			ImGui::EndMenuBar();
		}
		if (menu == 0)
		{
			bPlanetUpdate |= 
				ImGui::InputFloat("Atmosphere Thickness", &setting.atmosphereThickness, 1);
			bSunUpdate |= 
				ImGui::InputFloat("Planet Rotation Axis", &setting.planetRotationAxis, 0.f, 360.f, "%.2f");
			bSunUpdate |=
				ImGui::InputFloat("Latitude", &setting.latitude, -90.f, 90.f, "%.2f");
			ImGui::Separator();
			bSunUpdate |= 
				ImGui::SliderFloat("Month", &setting.month, 1.f, 13.f, "%.2f");
			bSunUpdate |= 
				ImGui::SliderFloat("Hour", &setting.hour, 0.f, 24.f, "%.2f");
		}
		if (menu == 1)
		{
			bCloudUpdate |= 
				ImGui::SliderFloat("Cloud Amount", &setting.cloudAmount, 0.f, 1.f, "%.2f");
			bCloudUpdate |=
				ImGui::SliderFloat("Cloud Brightness", &setting.cloudBrightness, 0.f, 4.f, "%.2f");
			if (ImGui::Button("Curve##CloudBrightness"))
			{
				bCloudUpdate = true;
				bCurveEditorOpen = true;
				curBezier = &setting.cloudBrightnessBezier;
				curveEditor.SetControlPoint(setting.cloudBrightnessBezier);
			}
		}
	}
	ImGui::EndChild();

	if (bCurveEditorOpen)
	{
		ImGui::SetNextWindowSize({ 400, 400 }, ImGuiCond_::ImGuiCond_Appearing);
		if (ImGui::Begin("CurveEditorRoot", &bCurveEditorOpen))
		{
			if (curveEditor.Draw("CurveEditor"))
			{
				*curBezier = curveEditor.GetControlPointsAsBezier();
				if (curBezier == &setting.cloudBrightnessBezier)
					bCloudUpdate = true;
			}
			ImGui::End();
		}
	}

	if (bPlanetUpdate)
		dirtyFlags |= WeatherDirtyFlag::Atmosphere;
	if (bSunUpdate)
		dirtyFlags |= WeatherDirtyFlag::Lighting;
	if (bCloudUpdate)
		dirtyFlags |= WeatherDirtyFlag::Cloud;
}
