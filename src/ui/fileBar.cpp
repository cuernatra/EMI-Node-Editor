#include "fileBar.h"
#include <string>

FileBar::FileBar() : m_height{elementSizes::fileBarHeight}
{
}

void FileBar::draw()
{   
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));

    ImGui::PushStyleColor(ImGuiCol_Button, colors::gray);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors::lightGray);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors::gray);

    if (ImGui::MenuItem("file",  NULL, false, false))
    {
        printf("file\n");
    }

    ImGui::SameLine(0, 55);

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}