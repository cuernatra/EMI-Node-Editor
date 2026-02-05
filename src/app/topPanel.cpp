#include "topPanel.h"

TopPanel::TopPanel() : m_height{elementSizes::topBarHeight}
{
}

void TopPanel::draw()
{   
    ImGui::SetNextWindowPos(ImVec2{0,0});
    ImGui::SetNextWindowSize({appConstants::windowWidth, m_height});

    ImGui::Begin(
        "topPanel",
        nullptr,
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize
    );
    
    ImGui::End();
}