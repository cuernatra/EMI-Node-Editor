#include "editorLayout.h"

#include "ui/theme.h"
#include <imgui-SFML.h>
#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <EMI/EMI.h>

EditorLayout::EditorLayout()
{
    m_topPanel.setFilesystemCallback([this]() {
        m_consolePanel.addLogText("Filesystem");
    });
    m_topPanel.setSettingsCallback([this]() {
        m_showSettingsOverlay = !m_showSettingsOverlay;
        if (m_showSettingsOverlay)
        {
            ++m_settingsWindowGeneration;
            m_forceSettingsFocus = true;
            m_consolePanel.addLogText("Settings opened.");
        }
    });
    m_topPanel.setPreviewCallback([this](bool enabled) {
        m_previewEnabled = enabled;
        if (!m_previewEnabled)
        {
            m_graphPreviewPanel.close();
        }
    });

    m_mainEditor.setCompileCallback([this]() {
        if (!m_previewEnabled)
            return;

        if (!m_graphPreviewPanel.isOpen())
            m_graphPreviewPanel.open();

        m_graphPreviewPanel.restartPlayback();

        // Draw first frame immediately so preview doesn't stay blank while
        // compile/runtime work is still running on the main thread.
        m_mainEditor.syncNodePositionsForPreview();
        m_graphPreviewPanel.update(m_mainEditor.getGraphState());
    });

    m_mainEditor.setCompileLogSink([this](const std::string& message)
    {
        m_consolePanel.addLogText(message);
    });

    m_consoleEmiLogger = std::make_unique<ConsoleEmiLogger>(m_consolePanel);
    EMI::SetCompileLog(m_consoleEmiLogger.get());
    EMI::SetRuntimeLog(m_consoleEmiLogger.get());
    EMI::SetScriptLog(m_consoleEmiLogger.get());

    m_consolePanel.addLog("Console initialized. Welcome to EMI Visual Programming Tool!");
}

EditorLayout::~EditorLayout()
{
}

void EditorLayout::draw()
{   
    bool drawInspectorOverlay = false;
    ImVec2 inspectorPos(0.0f, 0.0f);
    ImVec2 inspectorSize(0.0f, 0.0f);
    const ImVec2 panelPadding = layoutConstants::panelPadding;
    const ImVec2 panelItemSpacing = layoutConstants::panelItemSpacing;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);

    // Make window responsive
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);

    // Root layout window should have no padding/spacing so panel boundaries are flush.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, layoutConstants::rootWindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, layoutConstants::rootItemSpacing);
    ImGui::Begin(
        "EMI EDITOR", 
        nullptr, 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoCollapse | 
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse
    );

    const float totalWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    const float minLeft = layoutConstants::minLeftPanelWidth;
    const float minRight = layoutConstants::minInspectorPanelWidth;
    const float splitterWidth = layoutConstants::horizontalSplitterWidth;

    if(m_leftPanelWidth <= 0.0f)
    {
        m_leftPanelWidth = std::clamp(totalWidth * 0.25f, minLeft, totalWidth - minRight - splitterWidth);
    }

    if (m_rightPanelWidth <= 0.0f)
    {
        const float maxRight = std::max(minRight, totalWidth - m_leftPanelWidth - splitterWidth - minLeft);
        m_rightPanelWidth = std::clamp(totalWidth * 0.24f, minRight, maxRight);
    }

    // set new left panel width value to settings
    Settings::leftPanelWidth = m_leftPanelWidth;
    

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, panelPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, panelItemSpacing);
    ImGui::BeginChild("TOP BAR", ImVec2(totalWidth, elementSizes::topBarHeight), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    m_topPanel.draw();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, panelPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, panelItemSpacing);
    ImGui::BeginChild("NODE PALETTE", ImVec2(m_leftPanelWidth, 0), true);
    m_leftPanel.draw(m_mainEditor.hasStartNode());
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::SameLine(0.0f, 0.0f);

    DrawSplitter(totalWidth, splitterWidth, minLeft, minRight);

    ImGui::SameLine(0.0f, 0.0f);

    ImGui::BeginGroup();

    uintptr_t selectedNodeId = 0;
    const bool showInspector = m_mainEditor.tryGetSingleSelectedNodeId(selectedNodeId);
    if (showInspector)
    {
        if (selectedNodeId != m_lastInspectorNodeId)
        {
            m_lastInspectorNodeId = selectedNodeId;
            ++m_inspectorWindowGeneration;
        }
    }

    const float maxRightWidth = std::max(
        minRight,
        totalWidth - m_leftPanelWidth - splitterWidth - minLeft
    );
    const float inspectorWidth = std::clamp(m_rightPanelWidth, minRight, maxRightWidth);

    const float mainWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float rightColumnHeight = ImGui::GetContentRegionAvail().y;
    const float consoleSplitterThickness = layoutConstants::consoleSplitterThickness;
    const float minMainHeight = layoutConstants::minMainEditorHeight;
    const float minConsoleHeight = layoutConstants::minConsoleHeight;
    const bool consoleMinimized = m_consolePanel.isMinimized();

    if (!consoleMinimized)
    {
        const float maxConsoleHeight = std::max(
            minConsoleHeight,
            rightColumnHeight - minMainHeight - consoleSplitterThickness
        );

        // Initialize ratio from persisted pixel height once, then keep sizing
        // responsive by deriving height from ratio on every frame.
        if (m_consolePanelHeight <= 0.0f)
        {
            m_consolePanelHeight = 300.0f;
        }

        const float safeHeight = std::max(1.0f, rightColumnHeight);
        const float minRatio = minConsoleHeight / safeHeight;
        const float maxRatio = maxConsoleHeight / safeHeight;

        if (m_consolePanelRatio <= 0.0f)
        {
            const float clampedInitialHeight = std::clamp(m_consolePanelHeight, minConsoleHeight, maxConsoleHeight);
            m_consolePanelRatio = std::clamp(clampedInitialHeight / safeHeight, minRatio, maxRatio);
        }
        else
        {
            m_consolePanelRatio = std::clamp(m_consolePanelRatio, minRatio, maxRatio);
        }

        m_consolePanelHeight = std::clamp(safeHeight * m_consolePanelRatio, minConsoleHeight, maxConsoleHeight);
        m_consolePanel.setHeight(m_consolePanelHeight);
    }

    // Persist finalized console panel height every frame.
    Settings::consolePanelHeight = m_consolePanelHeight;

    const float activeConsoleSplitterThickness = consoleMinimized ? 0.0f : consoleSplitterThickness;
    const float mainHeight = rightColumnHeight - m_consolePanel.getHeight() - activeConsoleSplitterThickness;

    const ImVec2 mainEditorPos = ImGui::GetCursorScreenPos();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, panelPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, panelItemSpacing);
    ImGui::BeginChild("MAIN EDITOR", ImVec2(mainWidth, mainHeight), true);
    m_mainEditor.draw();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    if (!consoleMinimized)
    {
        DrawConsoleSplitter(
            mainWidth,
            consoleSplitterThickness,
            minMainHeight,
            minConsoleHeight,
            rightColumnHeight
        );
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, panelPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, panelItemSpacing);
    m_consolePanel.draw();
    ImGui::PopStyleVar(2);

    ImGui::EndGroup();

    if (showInspector)
    {
        const float inspectorPaddingX = layoutConstants::inspectorPaddingX;
        const float inspectorHeight = std::max(50.0f, rightColumnHeight);

        inspectorPos = ImVec2(
            mainEditorPos.x + mainWidth - inspectorWidth - inspectorPaddingX,
            mainEditorPos.y
        );
        inspectorSize = ImVec2(
            inspectorWidth,
            inspectorHeight
        );

        drawInspectorOverlay = true;
    }

    ImGui::End();
    ImGui::PopStyleVar(2);

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

        // Separate topmost inspector window (kept fixed in place).
        // Slight transparency only.
        const ImVec4 overlayBg(colors::background.x, colors::background.y, colors::background.z, 0.70f);
        const ImVec4 overlayBorder(colors::textSecondary.x, colors::textSecondary.y, colors::textSecondary.z, 0.95f);
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

    // Allow overlay windows (like Inspector) to reuse graph canvas shortcuts.
    // This keeps standard actions (e.g. Delete selected node/link) working even
    // when the overlay currently has keyboard focus.
    m_mainEditor.handleSharedShortcuts();

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
        if (m_forceSettingsFocus)
            ImGui::SetNextWindowFocus();
        m_forceSettingsFocus = false;

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
            const auto drawColorChannelSlider = [](const char* id, const char* label, float& value) -> bool
            {
                ImGui::PushID(id);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(label);
                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 10.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 16.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

                ImGui::SetNextItemWidth(-FLT_MIN);
                bool changed = ImGui::SliderFloat("##slider", &value, 0.0f, 1.0f, "%.2f");

                const ImVec2 min = ImGui::GetItemRectMin();
                const ImVec2 max = ImGui::GetItemRectMax();
                const float t = std::clamp(value, 0.0f, 1.0f);
                const float x = min.x + (max.x - min.x) * t;

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddLine(ImVec2(x, min.y + 2.0f), ImVec2(x, max.y - 2.0f), IM_COL32(255, 255, 255, 235), 2.0f);

                ImGui::PopStyleVar(3);
                ImGui::PopStyleColor(6);
                ImGui::PopID();
                return changed;
            };

            ImGui::TextUnformatted("Settings");
            ImGui::Separator();

            ImGui::TextUnformatted("Grid Colors");
            ImVec4 gridBgColor(
                Settings::gridBgColorR,
                Settings::gridBgColorG,
                Settings::gridBgColorB,
                Settings::gridBgColorA
            );
            ImGui::ColorButton("##grid_bg_preview", gridBgColor, ImGuiColorEditFlags_NoTooltip, ImVec2(46.0f, 22.0f));
            ImGui::SameLine();
            ImGui::TextUnformatted("Grid background");

            bool gridBgChanged = false;
            gridBgChanged |= drawColorChannelSlider("grid_bg_r", "RED", gridBgColor.x);
            gridBgChanged |= drawColorChannelSlider("grid_bg_g", "GREEN", gridBgColor.y);
            gridBgChanged |= drawColorChannelSlider("grid_bg_b", "BLUE", gridBgColor.z);
            gridBgChanged |= drawColorChannelSlider("grid_bg_a", "ALPHA", gridBgColor.w);

            if (gridBgChanged)
            {
                Settings::gridBgColorR = gridBgColor.x;
                Settings::gridBgColorG = gridBgColor.y;
                Settings::gridBgColorB = gridBgColor.z;
                Settings::gridBgColorA = gridBgColor.w;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextUnformatted("Grid Line Colors");

            ImVec4 gridLineColor(
                Settings::gridLineColorR,
                Settings::gridLineColorG,
                Settings::gridLineColorB,
                Settings::gridLineColorA
            );
            ImGui::ColorButton("##grid_line_preview", gridLineColor, ImGuiColorEditFlags_NoTooltip, ImVec2(46.0f, 22.0f));
            ImGui::SameLine();
            ImGui::TextUnformatted("Grid lines");

            bool gridLineChanged = false;
            gridLineChanged |= drawColorChannelSlider("grid_line_r", "RED", gridLineColor.x);
            gridLineChanged |= drawColorChannelSlider("grid_line_g", "GREEN", gridLineColor.y);
            gridLineChanged |= drawColorChannelSlider("grid_line_b", "BLUE", gridLineColor.z);
            gridLineChanged |= drawColorChannelSlider("grid_line_a", "ALPHA", gridLineColor.w);

            if (gridLineChanged)
            {
                Settings::gridLineColorR = gridLineColor.x;
                Settings::gridLineColorG = gridLineColor.y;
                Settings::gridLineColorB = gridLineColor.z;
                Settings::gridLineColorA = gridLineColor.w;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Values are saved to settings.json on app exit.");
            ImGui::End();
        }

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        style.Alpha = prevAlpha;
    }

    m_mainEditor.syncNodePositionsForPreview();
    m_graphPreviewPanel.update(m_mainEditor.getGraphState());
}

void EditorLayout::DrawSplitter(float totalWidth, float thickness, float minLeft, float minRight)
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

void EditorLayout::DrawConsoleSplitter(float width, float thickness, float minMain, float minConsole, float availableHeight)
{
    ImGui::InvisibleButton("console_splitter", ImVec2(width, thickness));

    if (ImGui::IsItemActive())
    {
        const float delta = ImGui::GetIO().MouseDelta.y;
        const float maxConsole = std::max(minConsole, availableHeight - minMain - thickness);
        const float newConsoleHeight = std::clamp(m_consolePanel.getHeight() - delta, minConsole, maxConsole);
        m_consolePanelHeight = newConsoleHeight; // Update member variable to persist height
        if (availableHeight > 0.0f)
        {
            m_consolePanelRatio = newConsoleHeight / availableHeight;
        }
        m_consolePanel.setHeight(newConsoleHeight);
    }

    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
}