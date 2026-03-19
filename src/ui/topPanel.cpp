#include "topPanel.h"
#include <string>

TopPanel::TopPanel() : m_height{elementSizes::topBarHeight}
{
}

void TopPanel::draw()
{   
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));

    ImGui::PushStyleColor(ImGuiCol_Button, colors::gray);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors::lightGray);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors::gray);

    if (ImGui::Button("test1", ImVec2(80, m_height - 14)))
    {
        printf("miau\n");
    }

    ImGui::SameLine(0, 55);

    if (ImGui::Button("test2", ImVec2(80, m_height - 14)))
    {
        m_openSettingsRequested = true;
    }

    ImGui::SameLine(0, 10);

    if (ImGui::Button("test3", ImVec2(80, m_height - 14)))
    {
        printf("test3\n");
    }

    ImGui::SameLine(0, 10);

    if (ImGui::Button("test4", ImVec2(80, m_height - 14)))
    {
        printf("test4\n");
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

bool TopPanel::consumeOpenSettingsRequested()
{
    const bool requested = m_openSettingsRequested;
    m_openSettingsRequested = false;
    return requested;
}