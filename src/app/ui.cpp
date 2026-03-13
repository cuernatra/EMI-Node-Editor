#include "ui.h"
#include <imgui-SFML.h>
#include <algorithm>

Ui::Ui() 
{
}

void Ui::draw()
{   
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

    ImGui::BeginChild("TOP BAR", ImVec2(totalWidth, elementSizes::topBarHeight), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    m_topPanel.draw();
    ImGui::EndChild();

    ImGui::BeginChild("NODE PALETTE", ImVec2(m_leftPanelWidth, 0), true);
    m_leftPanel.draw();
    ImGui::EndChild();

    ImGui::SameLine();

    DrawSplitter(totalWidth, splitterWidth, minLeft, minRight);

    ImGui::SameLine();

    const bool showInspector = m_mainEditor.hasSelectedNode();
    const float inspectorWidth = showInspector ? m_rightPanelWidth : 0.0f;

    float mainWidth = totalWidth - m_leftPanelWidth - splitterWidth - inspectorWidth;
    if (mainWidth < minLeft)
        mainWidth = minLeft;

    ImGui::BeginChild("MAIN EDITOR", ImVec2(mainWidth, 0), true);
    m_mainEditor.draw();
    ImGui::EndChild();

    if (showInspector)
    {
        ImGui::SameLine();
        ImGui::BeginChild("INSPECTOR", ImVec2(0, 0), true);
        m_mainEditor.drawInspectorPanel();
        ImGui::EndChild();
    }

    ImGui::End();
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