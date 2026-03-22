#include "fileBar.h"
#include "../editor/mainEditor.h"
#include <string>

FileBar::FileBar(MainEditor* editor) : m_height{elementSizes::fileBarHeight}, m_editor(editor)
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
                m_editor->NewGraph();
            }
            if(ImGui::MenuItem("Open"))
            {
                printf("Open\n");
                m_editor->OpenGraph();
            }
            if(ImGui::MenuItem("Save As"))
            {
                printf("Save As\n");
                m_editor->SaveGraph();
            }
            if(ImGui::MenuItem("Exit"))
            {
                printf("Exit\n");
                m_editor->Exit();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    ImGui::PopStyleVar();
}