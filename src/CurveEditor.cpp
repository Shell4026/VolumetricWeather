#include "CurveEditor.h"

CurveEditor::CurveEditor()
{
    m_controlPoints[0] = glm::vec2(0.0f, 0.0f);
    m_controlPoints[1] = glm::vec2(0.25f, 0.75f);
    m_controlPoints[2] = glm::vec2(0.5f, 0.5f);
    m_controlPoints[3] = glm::vec2(1.0f, 1.0f);

    SetSampleCount(100);
}

auto CurveEditor::Draw(const char* id, ImVec2 size) -> bool
{
    ImVec2 available = ImGui::GetContentRegionAvail();

    if (size.x <= 0.0f)
        size.x = available.x;

    if (size.y <= 0.0f)
        size.y = 300.0f;

    size.x = std::max(size.x, 50.0f);
    size.y = std::max(size.y, 50.0f);

    // ----------------------------------------------------
    // Widget 영역
    // ----------------------------------------------------

    const ImVec2 canvasMin =
        ImGui::GetCursorScreenPos();

    const ImVec2 canvasMax =
    {
        canvasMin.x + size.x,
        canvasMin.y + size.y
    };

    ImGui::InvisibleButton(
        id,
        size,
        ImGuiButtonFlags_MouseButtonLeft);

    const bool hovered =
        ImGui::IsItemHovered();

    ImDrawList* drawList =
        ImGui::GetWindowDrawList();

    bool changed = false;

    // ----------------------------------------------------
    // Mouse interaction
    // ----------------------------------------------------

    changed |= HandleInput(
        canvasMin,
        canvasMax,
        hovered);

    // 드래그 후 최신 위치로 sample 생성
    UpdateSamples();

    // ----------------------------------------------------
    // Rendering
    // ----------------------------------------------------

    drawList->PushClipRect(
        canvasMin,
        canvasMax,
        true);

    DrawBackground(
        drawList,
        canvasMin,
        canvasMax);

    DrawGrid(
        drawList,
        canvasMin,
        canvasMax);

    DrawControlLines(
        drawList,
        canvasMin,
        canvasMax);

    DrawCurve(
        drawList,
        canvasMin,
        canvasMax);

    DrawControlPoints(
        drawList,
        canvasMin,
        canvasMax);

    drawList->PopClipRect();

    return changed;
}

auto CurveEditor::Evaluate(float t) const -> glm::vec2
{
    Bezier bezier{};
    bezier.a = m_controlPoints[0];
    bezier.b = m_controlPoints[1];
    bezier.c = 2.f * m_controlPoints[2] - m_controlPoints[1];
    bezier.d = m_controlPoints[3];

    return bezier.GetSample(t);
}

void CurveEditor::SetControlPoint(const Bezier& bezier)
{
    m_controlPoints[0] = bezier.a;
    m_controlPoints[1] = bezier.b;
    m_controlPoints[2] = (bezier.b + bezier.c) * 0.5f;
    m_controlPoints[3] = bezier.d;

    UpdateSamples();
}

void CurveEditor::SetControlPoint(int index, const glm::vec2& point)
{
    if (index < 0 || index >= 4)
        return;

    m_controlPoints[index] = point;

    UpdateSamples();
}

void CurveEditor::SetSampleCount(int count)
{
    m_sampleCount =
        std::max(count, 2);

    m_samples.resize(m_sampleCount);

    UpdateSamples();
}

void CurveEditor::SetRange(float xMin, float xMax, float yMin, float yMax)
{
    if (xMax <= xMin)
        return;

    if (yMax <= yMin)
        return;

    m_xMin = xMin;
    m_xMax = xMax;

    m_yMin = yMin;
    m_yMax = yMax;
}

void CurveEditor::SetGridDivisions(int x, int y)
{
    m_gridX = std::max(x, 1);
    m_gridY = std::max(y, 1);
}

auto CurveEditor::GetControlPointsAsBezier() const -> Bezier
{
    const glm::vec2& a = m_controlPoints[0];
    const glm::vec2& b = m_controlPoints[1];
    const glm::vec2& c = 2.f * m_controlPoints[2] - b;
    const glm::vec2& d = m_controlPoints[3];
    return Bezier{ a,b,c,d };
}

auto CurveEditor::HandleInput(const ImVec2& canvasMin, const ImVec2& canvasMax, bool canvasHovered) -> bool
{
    bool changed = false;

    const ImVec2 mouse =
        ImGui::GetMousePos();

    // ----------------------------------------------------
    // Mouse Down
    // ----------------------------------------------------

    if (canvasHovered &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_draggingPoint = FindPointUnderMouse(
            mouse,
            canvasMin,
            canvasMax);
    }

    // ----------------------------------------------------
    // Drag
    // ----------------------------------------------------

    if (m_draggingPoint >= 0)
    {
        if (ImGui::IsMouseDown(
            ImGuiMouseButton_Left))
        {
            glm::vec2 p =
                ScreenToGraph(
                    mouse,
                    canvasMin,
                    canvasMax);

            // 그래프 영역 제한
            p = glm::clamp(
                p,
                glm::vec2(m_xMin, m_yMin),
                glm::vec2(m_xMax, m_yMax));

            glm::vec2& target =
                m_controlPoints[
                    m_draggingPoint];

            if (target.x != p.x ||
                target.y != p.y)
            {
                target = p;
                changed = true;
            }
        }
        else
        {
            m_draggingPoint = -1;
        }
    }

    return changed;
}

auto CurveEditor::FindPointUnderMouse(const ImVec2& mouse, const ImVec2& canvasMin, const ImVec2& canvasMax) const -> int
{
    // 실제 점보다 조금 넓게 클릭 가능
    const float hitRadius =
        m_pointRadius + 6.0f;

    const float hitRadiusSq =
        hitRadius * hitRadius;

    int nearest = -1;

    float nearestDistance =
        hitRadiusSq;

    for (int i = 0; i < 4; ++i)
    {
        ImVec2 screen =
            GraphToScreen(
                m_controlPoints[i],
                canvasMin,
                canvasMax);

        const float dx =
            screen.x - mouse.x;

        const float dy =
            screen.y - mouse.y;

        const float distance =
            dx * dx + dy * dy;

        if (distance <= nearestDistance)
        {
            nearestDistance =
                distance;

            nearest = i;
        }
    }

    return nearest;
}

void CurveEditor::UpdateSamples()
{
    if (m_samples.size() !=
        static_cast<size_t>(m_sampleCount))
    {
        m_samples.resize(
            m_sampleCount);
    }

    for (int i = 0;
        i < m_sampleCount;
        ++i)
    {
        const float t =
            static_cast<float>(i) /
            static_cast<float>(
                m_sampleCount - 1);

        m_samples[i] =
            Evaluate(t);
    }
}

auto CurveEditor::GraphToScreen(const glm::vec2& point, const ImVec2& canvasMin, const ImVec2& canvasMax) const -> ImVec2
{
    const float tx =
        (point.x - m_xMin) /
        (m_xMax - m_xMin);

    const float ty =
        (point.y - m_yMin) /
        (m_yMax - m_yMin);

    return
    {
        canvasMin.x +
            tx *
            (canvasMax.x - canvasMin.x),

        canvasMax.y -
            ty *
            (canvasMax.y - canvasMin.y)
    };
}

auto CurveEditor::ScreenToGraph(const ImVec2& screen, const ImVec2& canvasMin, const ImVec2& canvasMax) const -> glm::vec2
{
    const float tx =
        (screen.x - canvasMin.x) /
        (canvasMax.x - canvasMin.x);

    const float ty =
        (canvasMax.y - screen.y) /
        (canvasMax.y - canvasMin.y);

    return glm::vec2(
        m_xMin +
        tx *
        (m_xMax - m_xMin),

        m_yMin +
        ty *
        (m_yMax - m_yMin));
}

void CurveEditor::DrawBackground(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) const
{
    drawList->AddRectFilled(
        min,
        max,
        IM_COL32(
            25,
            25,
            28,
            255));

    drawList->AddRect(
        min,
        max,
        IM_COL32(
            90,
            90,
            95,
            255));
}

void CurveEditor::DrawGrid(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) const

{
    const ImU32 gridColor =
        IM_COL32(
            70,
            70,
            75,
            100);

    const ImU32 axisColor =
        IM_COL32(
            130,
            130,
            135,
            180);

    // ----------------------------------------------------
    // vertical
    // ----------------------------------------------------

    for (int i = 1;
        i < m_gridX;
        ++i)
    {
        const float t =
            static_cast<float>(i) /
            static_cast<float>(
                m_gridX);

        const float x =
            min.x +
            (max.x - min.x) * t;

        drawList->AddLine(
            ImVec2(x, min.y),
            ImVec2(x, max.y),
            gridColor);
    }

    // ----------------------------------------------------
    // horizontal
    // ----------------------------------------------------

    for (int i = 1;
        i < m_gridY;
        ++i)
    {
        const float t =
            static_cast<float>(i) /
            static_cast<float>(
                m_gridY);

        const float y =
            min.y +
            (max.y - min.y) * t;

        drawList->AddLine(
            ImVec2(min.x, y),
            ImVec2(max.x, y),
            gridColor);
    }

    // ----------------------------------------------------
    // X = 0
    // ----------------------------------------------------

    if (m_xMin <= 0.0f &&
        m_xMax >= 0.0f)
    {
        ImVec2 bottom =
            GraphToScreen(
                glm::vec2(0.0f, m_yMin),
                min,
                max);

        ImVec2 top =
            GraphToScreen(
                glm::vec2(0.0f, m_yMax),
                min,
                max);

        drawList->AddLine(
            bottom,
            top,
            axisColor,
            1.5f);
    }

    // ----------------------------------------------------
    // Y = 0
    // ----------------------------------------------------

    if (m_yMin <= 0.0f &&
        m_yMax >= 0.0f)
    {
        ImVec2 left =
            GraphToScreen(
                glm::vec2(m_xMin, 0.0f),
                min,
                max);

        ImVec2 right =
            GraphToScreen(
                glm::vec2(m_xMax, 0.0f),
                min,
                max);

        drawList->AddLine(
            left,
            right,
            axisColor,
            1.5f);
    }
}

void CurveEditor::DrawControlLines(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) const
{

    {
        const ImU32 color =
            IM_COL32(
                150,
                150,
                150,
                170);

        const ImVec2 p0 =
            GraphToScreen(
                m_controlPoints[0],
                min,
                max);

        const ImVec2 p1 =
            GraphToScreen(
                m_controlPoints[1],
                min,
                max);

        const ImVec2 p2 =
            GraphToScreen(
                m_controlPoints[2],
                min,
                max);

        const glm::vec2 q =
            2.0f * m_controlPoints[2] -
            m_controlPoints[1];
        const ImVec2 p3 =
            GraphToScreen(
                q,
                min,
                max);

        const ImVec2 p4 =
            GraphToScreen(
                m_controlPoints[3],
                min,
                max);

        drawList->AddLine(
            p1,
            p2,
            color,
            1.0f);

        drawList->AddLine(
            p2,
            p3,
            color,
            1.0f);
    }
}

void CurveEditor::DrawCurve(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) const

{
    if (m_samples.size() < 2)
        return;

    const ImU32 curveColor =
        IM_COL32(
            80,
            220,
            120,
            255);

    ImVec2 previous =
        GraphToScreen(
            m_samples[0],
            min,
            max);

    // AddPolyline 대신 AddLine 반복.
    //
    // 이렇게 하면 Dear ImGui 버전별
    // AddPolyline 파라미터 순서 차이에도
    // 영향을 덜 받는다.
    for (size_t i = 1;
        i < m_samples.size();
        ++i)
    {
        const ImVec2 current =
            GraphToScreen(
                m_samples[i],
                min,
                max);

        drawList->AddLine(
            previous,
            current,
            curveColor,
            m_curveThickness);

        previous = current;
    }
}

void CurveEditor::DrawControlPoints(ImDrawList* drawList, const ImVec2& min, const ImVec2& max) const

{
    const ImVec2 mouse =
        ImGui::GetMousePos();

    for (int i = 0; i < 4; ++i)
    {
        const ImVec2 position =
            GraphToScreen(
                m_controlPoints[i],
                min,
                max);

        const float dx =
            mouse.x - position.x;

        const float dy =
            mouse.y - position.y;

        const bool hovered =
            (dx * dx + dy * dy) <=
            (m_pointRadius + 5.0f) *
            (m_pointRadius + 5.0f);

        const bool active =
            m_draggingPoint == i;

        float radius =
            m_pointRadius;

        if (hovered)
            radius += 2.0f;

        if (active)
            radius += 3.0f;

        ImU32 color;

        // P0 / P3
        if (i == 0 || i == 3)
        {
            color =
                IM_COL32(
                    60,
                    170,
                    255,
                    255);
        }

        // P1 / P2
        else
        {
            color =
                IM_COL32(
                    255,
                    170,
                    50,
                    255);
        }

        drawList->AddCircleFilled(
            position,
            radius,
            color);

        drawList->AddCircle(
            position,
            radius,
            IM_COL32(
                255,
                255,
                255,
                255),
            0,
            1.0f);
    }
}