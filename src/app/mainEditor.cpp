#include "mainEditor.h"

MainEditor::MainEditor() 
    : m_width{appConstants::windowWidth}
    , m_height{appConstants::windowheight}
    , m_position{0,0}
{
}

void MainEditor::draw()
{
    ImGui::SetNextWindowPos(ImVec2{m_position.x, m_position.y});
    ImGui::SetNextWindowSize({m_width, m_height});

    ImGui::SetNextWindowBgAlpha(0.0f);
    
    ImGui::Begin(
        "mainEditor",
        nullptr,
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize
    );

    ImDrawList* bg = ImGui::GetBackgroundDrawList();
    bg->AddRectFilled(
        ImVec2(m_position.x, m_position.y),
        ImVec2(m_position.x + m_width, m_position.y + m_height),
        colors::test
    );

    ImGui::End();
}