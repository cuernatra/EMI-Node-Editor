#include "mainEditor.h"

MainEditor::MainEditor() 
{
}

void MainEditor::draw()
{   
    ImGui::Text("EMI-EDITOR");
    ImGui::Separator();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();

    drawList->AddRectFilled(
        pos, 
        ImVec2(pos.x + size.x, pos.y + size.y), 
        colors::blue
    );
}