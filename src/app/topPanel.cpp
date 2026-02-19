#include "topPanel.h"

TopPanel::TopPanel() : m_height{elementSizes::topBarHeight}
{
}

void TopPanel::draw()
{   
    ImGui::SetCursorPos(ImVec2{0, 0});
    ImGui::Dummy(ImVec2{appConstants::windowWidth, m_height});
}