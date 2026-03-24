#include "ui.h"
#include <imgui-SFML.h>
#include <algorithm>
#include <cstdio>
#include <EMI/EMI.h>

Ui::Ui()
{
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

Ui::~Ui()
{
}

void Ui::draw()
{   
    bool drawInspectorOverlay = false;
    ImVec2 inspectorPos(0.0f, 0.0f);
    ImVec2 inspectorSize(0.0f, 0.0f);

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);

    // Make window responsive
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);

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
    ImGui::EndChild();

    ImGui::BeginChild("NODE PALETTE", ImVec2(m_leftPanelWidth, 0), true);
    m_leftPanel.draw(m_mainEditor.hasStartNode());
    ImGui::EndChild();

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

    const float mainWidth = totalWidth - m_leftPanelWidth - splitterWidth;
    const float rightColumnHeight = ImGui::GetContentRegionAvail().y;
    const float consoleSplitterThickness = 5.0f;
    const float minMainHeight = 80.0f;
    const float minConsoleHeight = 60.0f;
    const bool consoleMinimized = m_consolePanel.isMinimized();

    if (!consoleMinimized)
    {
        const float maxConsoleHeight = std::max(
            minConsoleHeight,
            rightColumnHeight - minMainHeight - consoleSplitterThickness
        );
        m_consolePanel.setHeight(std::clamp(m_consolePanel.getHeight(), minConsoleHeight, maxConsoleHeight));
    }

    const float activeConsoleSplitterThickness = consoleMinimized ? 0.0f : consoleSplitterThickness;
    const float mainHeight = rightColumnHeight - m_consolePanel.getHeight() - activeConsoleSplitterThickness;

    const ImVec2 mainEditorPos = ImGui::GetCursorScreenPos();

    ImGui::BeginChild("MAIN EDITOR", ImVec2(mainWidth, mainHeight), true);
    m_mainEditor.draw();
    ImGui::EndChild();

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

    m_consolePanel.draw();

    ImGui::EndGroup();

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

    // Allow overlay windows (like Inspector) to reuse graph canvas shortcuts.
    // This keeps legacy actions (e.g. Delete selected node/link) working even
    // when the overlay currently has keyboard focus.
    m_mainEditor.handleSharedShortcuts();
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

void Ui::DrawConsoleSplitter(float width, float thickness, float minMain, float minConsole, float availableHeight)
{
    ImGui::InvisibleButton("console_splitter", ImVec2(width, thickness));

    if (ImGui::IsItemActive())
    {
        const float delta = ImGui::GetIO().MouseDelta.y;
        const float maxConsole = std::max(minConsole, availableHeight - minMain - thickness);
        const float newConsoleHeight = std::clamp(m_consolePanel.getHeight() - delta, minConsole, maxConsole);
        m_consolePanel.setHeight(newConsoleHeight);
    }

    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
}