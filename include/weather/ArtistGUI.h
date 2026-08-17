#pragma once
#include "Setting.h"
#include "IGUI.h"
#include "CurveEditor.h"

class ArtistGUI : public IGUI
{
public:
	ArtistGUI(ArtistSetting& setting);

	virtual void RenderGUI() override;
	virtual auto GetName() const -> const std::string & override { return name; }
	
	auto GetDirtyFlags() const -> WeatherDirtyFlags { return dirtyFlags; }
private:
	ArtistSetting& setting;

	const std::string name{ "ArtistGUI" };

	int menu = -1;
	WeatherDirtyFlags dirtyFlags = 0;

	CurveEditor curveEditor;

	Bezier* curBezier = nullptr;
	bool bCurveEditorOpen = false;
};