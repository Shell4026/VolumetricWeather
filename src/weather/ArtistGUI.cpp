#include "Weather/ArtistGUI.h"

#include "core/Logger.h"
#include "core/Util.h"

#include <imgui/imgui.h>

void DrawTooltip(const char* description)
{
	if (!ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		return;

	ImGui::BeginTooltip();
	ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.f);
	ImGui::TextUnformatted(description);
	ImGui::PopTextWrapPos();
	ImGui::EndTooltip();
}

ArtistGUI::ArtistGUI(ArtistSetting& setting) :
	setting(setting)
{
}

void ArtistGUI::RenderGUI()
{
	using namespace util;
	dirtyFlags = 0;
	bool bPlanetUpdate = false;
	bool bSunUpdate = false;
	bool bCloudUpdate = false;
	if (ImGui::BeginChild("ArtistGUIChild", ImVec2{ 0, 0 }, ImGuiChildFlags_::ImGuiChildFlags_Borders, ImGuiWindowFlags_::ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::Button(U8Str(u8"행성")))
				menu = menu == 0 ? -1 : 0;
			if (ImGui::Button(U8Str(u8"구름")))
				menu = menu == 1 ? -1 : 1;
			ImGui::EndMenuBar();
		}
		if (menu == 0)
		{
			ImGui::TextUnformatted(U8Str(u8"대기"));
			bPlanetUpdate |=
				ImGui::SliderFloat(U8Str(u8"대기 두께"), &setting.atmosphereThickness, 0.1f, 4.f, "%.2fx");
			DrawTooltip(U8Str(u8"대기 두께의 배율입니다. 1.0x은 지구의 100km 대기와 같습니다."));

			ImGui::SeparatorText(U8Str(u8"대기 산란"));
			bPlanetUpdate |=
				ImGui::SliderFloat(U8Str(u8"레일리 산란 강도"), &setting.rayleighScatteringStrength, 0.f, 4.f, "%.2fx");
			DrawTooltip(U8Str(u8"공기 분자에 의한 산란 강도입니다. 값을 높이면 낮의 푸른 하늘과 일출, 일몰의 붉은빛이 더 뚜렷해집니다."));
			bPlanetUpdate |=
				ImGui::SliderFloat(U8Str(u8"미 산란 강도"), &setting.mieScatteringStrength, 0.f, 100.f, "%.2fx");
			DrawTooltip(U8Str(u8"먼지나 수증기 같은 미세 입자에 의한 산란 강도입니다. 값을 높이면 대기가 더 뿌옇고 태양 주변의 후광이 강해집니다."));
			bPlanetUpdate |=
				ImGui::SliderFloat(U8Str(u8"미 산란 방향성"), &setting.mieAnisotropy, 0.f, 0.99f, "%.2f");
			DrawTooltip(U8Str(u8"산란광이 태양 방향에 얼마나 집중되는지를 정합니다. 0은 모든 방향이 비슷하고, 1에 가까울수록 태양 주변에 빛이 집중됩니다."));

			ImGui::SeparatorText(U8Str(u8"태양"));
			bSunUpdate |=
				ImGui::SliderFloat(U8Str(u8"밝기"), &setting.sunIntensity, 0.f, 4.f, "%.2fx");
			bSunUpdate |=
				ImGui::SliderFloat(U8Str(u8"자전축"), &setting.planetRotationAxis, 0.f, 45.f, U8Str(u8"%.1f도"));
			DrawTooltip(U8Str(u8"행성의 기울기입니다. 지구의 자전축은 23.4도입니다."));
			bSunUpdate |=
				ImGui::SliderFloat(U8Str(u8"위도"), &setting.latitude, -90.f, 90.f, U8Str(u8"%.1f도"));
			DrawTooltip(U8Str(u8"카메라의 지구상의 위도 위치입니다."));
			bSunUpdate |=
				ImGui::SliderFloat(U8Str(u8"월"), &setting.month, 1.f, 12.f, "%.0f");
			DrawTooltip(U8Str(u8"계절을 바꿔 태양의 경로를 바꿉니다."));
			bSunUpdate |=
				ImGui::SliderFloat(U8Str(u8"시간"), &setting.hour, 0.f, 24.f, U8Str(u8"%.1f시"));
			DrawTooltip(U8Str(u8"현지 시각입니다. 12시는 정오이고 0시와 24시는 자정입니다."));
		}
		if (menu == 1)
		{
			ImGui::SeparatorText(U8Str(u8"모양"));
			bCloudUpdate |=
				ImGui::SliderFloat(U8Str(u8"구름 양"), &setting.cloudAmount, 0.f, 1.f, "%.2f");
			DrawTooltip(U8Str(u8"구름이 하늘을 얼마나 덮을지를 결정합니다."));
			bCloudUpdate |=
				ImGui::SliderFloat(U8Str(u8"수직 발달"), &setting.cloudVerticalAmount, -1.f, 1.f, "%.2f");
			DrawTooltip(U8Str(u8"-1에 가까우면 구름 하단이, 1에 가까우면 구름 상단이 풍성해집니다."));
			bCloudUpdate |=
				ImGui::SliderFloat(U8Str(u8"구름 크기"), &setting.cloudSizeKM, 10.f, 100.f, "%.0f km");
			DrawTooltip(U8Str(u8"구름 덩어리 하나의 대략적인 크기입니다."));
			bCloudUpdate |=
				ImGui::SliderFloat(U8Str(u8"디테일"), &setting.cloudDetail, 0.f, 1.f, "%.2f");
			DrawTooltip(U8Str(u8"구름의 외곽에 디테일을 추가합니다. 1에 가까울수록 솜뭉치 같은 느낌을 냅니다."));
			bCloudUpdate |=
				ImGui::SliderFloat(U8Str(u8"밀도"), &setting.cloudDensity, 0.f, 1.f, "%.2f");
			DrawTooltip(U8Str(u8"구름의 밀도가 높을수록 구름이 불투명해집니다."));

			ImGui::SeparatorText(U8Str(u8"조명"));
			bCloudUpdate |=
				ImGui::SliderFloat(U8Str(u8"구름 밝기"), &setting.cloudBrightness, 0.f, 4.f, "%.2f");
			DrawTooltip(U8Str(u8"구름 내부에서 산란되는 빛의 밝기를 조절합니다."));
			if (ImGui::Button(U8Str(u8"높이 커브##CloudBrightness")))
			{
				bCurveEditorOpen = true;
				curBezier = &setting.cloudBrightnessBezier;
				curveEditor.SetControlPoint(setting.cloudBrightnessBezier);
			}
			DrawTooltip(U8Str(u8"높이에 따라 구름의 밝기를 커브로 상세하게 설정합니다."));
			bCloudUpdate |=
				ImGui::SliderFloat(U8Str(u8"파우더 효과"), &setting.cloudSoftness, 0.f, 1.f, "%.2f");
			DrawTooltip(U8Str(u8"구름 모서리 부분의 어두움을 나타냅니다. 태양의 반대 방향일수록 잘 보입니다."));

			ImGui::SeparatorText(U8Str(u8"바람"));
			bCloudUpdate |=
				ImGui::SliderFloat(U8Str(u8"풍향"), &setting.windDirectionDegrees, 0.f, 360.f, U8Str(u8"%.0f도"));
			DrawTooltip(U8Str(u8"구름이 어느 방향으로 움직일지 정합니다."));
			bCloudUpdate |=
				ImGui::SliderFloat(U8Str(u8"풍속"), &setting.windSpeedKmh, 0.f, 500.f, "%.0f km/h");
			DrawTooltip(U8Str(u8"구름이 움직이는 속도를 지정합니다."));
		}
	}
	ImGui::EndChild();

	if (bCurveEditorOpen && curBezier != nullptr)
	{
		ImGui::SetNextWindowSize({ 400, 400 }, ImGuiCond_::ImGuiCond_Appearing);
		if (ImGui::Begin(U8Str(u8"높이에 따른 구름 밝기"), &bCurveEditorOpen))
		{
			ImGui::TextUnformatted(U8Str(u8"구름 하단 (왼쪽) -> 구름 상단 (오른쪽)"));
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
