#include "ui.h"
#include <imgui-SFML.h>
#include <algorithm>
#include <cstdio>

Ui::Ui() 
{
}

void Ui::draw()
{   
    bool drawInspectorOverlay = false;
    bool forceInspectorFocus = false;
    ImVec2 inspectorPos(0.0f, 0.0f);
    ImVec2 inspectorSize(0.0f, 0.0f);
    bool forceSettingsFocus = false;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);

    // Make window responsive
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);

    ImGui::Begin(
        "EMI EDITOR", 
        nullptr, 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoCollapse | 
        ImGuiWindowFlags_NoTitleBar
    );

    const float totalWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    const float minLeft = 50.f;
    const float minRight = 200.f;
    const float splitterWidth = 5.f;

    if(m_leftPanelWidth <= 0.0f)
    {
        m_leftPanelWidth = std::clamp(totalWidth * 0.25f, minLeft, totalWidth - minRight - splitterWidth);
    }

    if (m_rightPanelWidth <= 0.0f)
    {
        const float maxRight = std::max(minRight, totalWidth - m_leftPanelWidth - splitterWidth - minLeft);
        m_rightPanelWidth = std::clamp(totalWidth * 0.24f, minRight, maxRight);
    }

    // save new left panel width value to settings
    Settings::leftPanelWidth = m_leftPanelWidth;

    ImGui::BeginChild("TOP BAR", ImVec2(totalWidth, elementSizes::topBarHeight), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    m_topPanel.draw();
    if (m_topPanel.consumeOpenSettingsRequested())
    {
        m_showSettingsOverlay = !m_showSettingsOverlay;
        if (m_showSettingsOverlay)
        {
            ++m_settingsWindowGeneration;
            forceSettingsFocus = true;
        }
    }
    ImGui::EndChild();

    ImGui::BeginChild("NODE PALETTE", ImVec2(m_leftPanelWidth, 0), true);
    m_leftPanel.draw(m_mainEditor.hasStartNode());
    ImGui::EndChild();

    ImGui::SameLine();

    DrawSplitter(totalWidth, splitterWidth, minLeft, minRight);

    ImGui::SameLine();

    uintptr_t selectedNodeId = 0;
    const bool showInspector = m_mainEditor.tryGetSingleSelectedNodeId(selectedNodeId);
    if (showInspector)
    {
        if (selectedNodeId != m_lastInspectorNodeId)
        {
            m_lastInspectorNodeId = selectedNodeId;
            ++m_inspectorWindowGeneration;
            forceInspectorFocus = true;
        }
        else
        {
            // Same node clicked again -> keep current inspector state, do nothing.
            forceInspectorFocus = false;
        }
    }

    const float maxRightWidth = std::max(
        minRight,
        totalWidth - m_leftPanelWidth - splitterWidth - minLeft
    );
    const float inspectorWidth = std::clamp(m_rightPanelWidth, minRight, maxRightWidth);

    float mainWidth = totalWidth - m_leftPanelWidth - splitterWidth;
    if (mainWidth < minLeft)
        mainWidth = minLeft;

    const ImVec2 mainEditorPos = ImGui::GetCursorScreenPos();

    ImGui::BeginChild("MAIN EDITOR", ImVec2(mainWidth, 0), true);
    m_mainEditor.draw();
    ImGui::EndChild();

    if (showInspector)
    {
        const float inspectorPaddingX = 8.0f;
        const ImVec2 mainEditorSize = ImGui::GetItemRectSize();

        inspectorPos = ImVec2(
            mainEditorPos.x + mainEditorSize.x - inspectorWidth - inspectorPaddingX,
            mainEditorPos.y
        );
        inspectorSize = ImVec2(
            inspectorWidth,
            std::max(50.0f, mainEditorSize.y)
        );

        drawInspectorOverlay = true;
    }

    ImGui::End();

    if (drawInspectorOverlay)
    {
        // Guard against leaked global alpha from other UI paths:
        // keep inspector always as visible as first open.
        ImGuiStyle& style = ImGui::GetStyle();
        const float prevAlpha = style.Alpha;
        style.Alpha = 1.0f;

        ImGui::SetNextWindowPos(inspectorPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(inspectorSize, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.70f);
        if (forceInspectorFocus)
            ImGui::SetNextWindowFocus();

        // Separate topmost inspector window (kept fixed in place).
        // Slight transparency only.
        const ImVec4 overlayBg(0.04f, 0.04f, 0.06f, 0.70f);
        const ImVec4 overlayBorder(0.35f, 0.35f, 0.35f, 0.95f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, overlayBg);
        ImGui::PushStyleColor(ImGuiCol_Border, overlayBorder);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

        char inspectorWindowName[96];
        std::snprintf(
            inspectorWindowName,
            sizeof(inspectorWindowName),
            "##INSPECTOR_OVERLAY_WINDOW_%u",
            static_cast<unsigned>(m_inspectorWindowGeneration)
        );

        ImGui::Begin(
            inspectorWindowName,
            nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings
        );
        m_mainEditor.drawInspectorPanel();
        ImGui::End();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);

        style.Alpha = prevAlpha;
    }

    if (m_showSettingsOverlay)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        const float prevAlpha = style.Alpha;
        style.Alpha = 1.0f;

        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float windowPadding = 20.0f;
        const float minTopY = elementSizes::topBarHeight + 8.0f;
        const float maxSettingsWidth = std::max(120.0f, display.x - windowPadding * 2.0f);
        const float maxSettingsHeight = std::max(120.0f, display.y - minTopY - windowPadding);
        const ImVec2 settingsSize(
            std::clamp(display.x * 0.90f, 200.0f, maxSettingsWidth),
            std::clamp(display.y * 0.90f, 200.0f, maxSettingsHeight)
        );
        const ImVec2 settingsPos(
            std::max(windowPadding, display.x - settingsSize.x - windowPadding),
            std::max(minTopY, display.y - settingsSize.y - windowPadding)
        );

        ImGui::SetNextWindowPos(settingsPos, ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(settingsSize, ImGuiCond_Appearing);
        ImGui::SetNextWindowBgAlpha(0.70f);
        if (forceSettingsFocus)
            ImGui::SetNextWindowFocus();

        const ImVec4 overlayBg(0.04f, 0.04f, 0.06f, 0.70f);
        const ImVec4 overlayBorder(0.35f, 0.35f, 0.35f, 0.95f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, overlayBg);
        ImGui::PushStyleColor(ImGuiCol_Border, overlayBorder);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

        char settingsWindowName[96];
        std::snprintf(
            settingsWindowName,
            sizeof(settingsWindowName),
            "Settings##SETTINGS_OVERLAY_WINDOW_%u",
            static_cast<unsigned>(m_settingsWindowGeneration)
        );

        if (!ImGui::Begin(settingsWindowName, &m_showSettingsOverlay, ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::End();
        }
        else
        {
            ImGui::TextUnformatted("Settings panel placeholder");
            ImGui::Separator();
            ImGui::TextDisabled("Opened from the top-bar test2 button.");
            ImGui::End();
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        style.Alpha = prevAlpha;
    }
}

void Ui::DrawSplitter(float totalWidth, float thickness, float minLeft, float minRight)
{   
    ImGui::InvisibleButton(
        "splitter",
        ImVec2(thickness, ImGui::GetContentRegionAvail().y)
    );

    // If you hold left mouse button on the splitter
    if(ImGui::IsItemActive())
    {   
        float delta = ImGui::GetIO().MouseDelta.x;

        float newLeft = m_leftPanelWidth + delta;
        float maxLeft = totalWidth - minRight - thickness;

        // Check that m_leftPanelWidth is between minLeft and maxLeft
        m_leftPanelWidth = std::clamp(newLeft, minLeft, maxLeft);
    }
    
    // Change cursor to <-> when over the splitter
    if(ImGui::IsItemHovered() || ImGui::IsItemActive())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
}