// GPT 5.6 sol로 작성
#pragma once
#include "Bezier.hpp"

#include <imgui/imgui.h>

#include <glm/glm.hpp>
#include <vector>
#include <algorithm>
#include <cmath>

class CurveEditor
{
public:
    CurveEditor();

    auto Draw(const char* id, ImVec2 size = ImVec2(0.0f, 300.0f)) -> bool;

    auto Evaluate(float t) const -> glm::vec2;

    void SetControlPoint(const Bezier& bezier);
    void SetControlPoint(int index, const glm::vec2& point);
    void SetSampleCount(int count);
    void SetRange(float xMin, float xMax, float yMin, float yMax);
    void SetGridDivisions(int x, int y);
    void SetPointRadius(float radius) { m_pointRadius = radius; }
    void SetCurveThickness(float thickness) { m_curveThickness = thickness; }

    auto GetControlPoint(int index) -> glm::vec2& { return m_controlPoints[index]; }
    auto GetControlPoint(int index) const -> const glm::vec2& { return m_controlPoints[index]; }
    auto GetControlPoints() const -> const glm::mat4x2& { return m_controlPoints; }
    auto GetControlPointsAsBezier() const -> Bezier;
    auto GetSamples() const -> const std::vector<glm::vec2>& { return m_samples; }
    auto GetSampleCount() const -> int { return m_sampleCount; }
private:
    auto HandleInput(const ImVec2& canvasMin, const ImVec2& canvasMax, bool canvasHovered) -> bool;
    auto FindPointUnderMouse(const ImVec2& mouse, const ImVec2& canvasMin, const ImVec2& canvasMax) const -> int;
    void UpdateSamples();
    auto GraphToScreen(const glm::vec2& point, const ImVec2& canvasMin, const ImVec2& canvasMax) const -> ImVec2;
    auto ScreenToGraph(const ImVec2& screen, const ImVec2& canvasMin, const ImVec2& canvasMax) const -> glm::vec2;

    void DrawBackground(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) const;
    void DrawGrid(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) const;
    void DrawControlLines(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) const;
    void DrawCurve(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) const;
    void DrawControlPoints(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) const;
private:
    glm::mat4x2 m_controlPoints;

    std::vector<glm::vec2> m_samples;

    int m_sampleCount = 100;

    int m_draggingPoint = -1;

    float m_xMin = 0.0f;
    float m_xMax = 1.0f;

    float m_yMin = 0.0f;
    float m_yMax = 1.0f;

    int m_gridX = 10;
    int m_gridY = 10;

    float m_pointRadius = 6.0f;
    float m_curveThickness = 2.0f;
};