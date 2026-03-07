#include "fileBar.h"
#include <string>

FileBar::FileBar() : m_height{elementSizes::fileBarHeight}
{
}

void FileBar::draw()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if(ImGui::BeginMenuBar())
    {
        if(ImGui::BeginMenu("File"))
        {
            ImGui::MenuItem("File menu",  NULL, false, false);
            if(ImGui::MenuItem("New"))
            {
                printf("New\n");
            }
            if(ImGui::MenuItem("Open"))
            {
                printf("Open\n");
            }
            if(ImGui::MenuItem("Save"))
            {
                printf("Save\n");
            }
            if(ImGui::MenuItem("Exit"))
            {
                printf("Exit\n");
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::PopStyleVar();
}