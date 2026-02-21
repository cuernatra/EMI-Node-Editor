#include "topPanel.h"

TopPanel::TopPanel() : m_height{elementSizes::topBarHeight}
{
}

void TopPanel::draw()
{   
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3);

    ImGui::PushStyleColor(ImGuiCol_Button, colors::gray);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors::lightGray);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors::gray);

    if (ImGui::Button("test1", ImVec2(80, m_height - 16)))
    {
    }

    ImGui::SameLine(0, 5);

    if (ImGui::Button("test2", ImVec2(80, m_height - 16)))
    {
    }

    ImGui::PopStyleColor(3);

    ImGui::PopStyleVar();
}