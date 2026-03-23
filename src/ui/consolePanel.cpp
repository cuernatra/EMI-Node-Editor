#include "consolePanel.h"
#include <string>
#include <iostream>

ConsolePanel::ConsolePanel() : m_height{300} // default height for console panel
{
}

void ConsolePanel::draw()
{   
    std::cout << "Drawing console panel..." << std::endl;
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
        printf("test2\n");
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