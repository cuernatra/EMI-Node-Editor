#include "ui.h"
#include <imgui-SFML.h>
#include <algorithm>

Ui::Ui(): m_mainEditor(),m_fileBar(&m_mainEditor)
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
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_MenuBar
    );

    const float totalWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    const float minLeft = 50.f;
    const float minRight = 200.f;

    if(m_leftPanelWidth <= 0.0f)
    {
        m_leftPanelWidth = std::clamp(totalWidth * 0.25f, minLeft, totalWidth - minRight - 5.f);
    } 

    ImGui::BeginChild("FILE BAR", ImVec2(totalWidth, elementSizes::fileBarHeight), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    | ImGuiWindowFlags_MenuBar);
    m_fileBar.draw();
    ImGui::EndChild();
    
    ImGui::BeginChild("TOP BAR", ImVec2(totalWidth, elementSizes::topBarHeight), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    m_topPanel.draw();
    ImGui::EndChild();

    ImGui::BeginChild("NODE PALETTE", ImVec2(m_leftPanelWidth, 0), true);
    m_leftPanel.draw();
    ImGui::EndChild();

    ImGui::SameLine();

    DrawSplitter(totalWidth, 5.f, 50.f, 200.f);

    ImGui::SameLine();

    ImGui::BeginChild("MAIN EDITOR", ImVec2(0,0), true);
    m_mainEditor.draw();
    ImGui::EndChild();

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