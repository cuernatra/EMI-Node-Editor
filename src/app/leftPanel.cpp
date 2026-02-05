#include "leftPanel.h"

LeftPanel::LeftPanel() 
    : m_width{appConstants::windowWidth / 5}
    , m_height{appConstants::windowheight}
    , m_position{0,0}
    , m_open{true}
{
    float width = elementSizes::dropBarWidth;
    float height = elementSizes::dropBarHeight;

    m_dropBars_A.emplace_back("test_1", 0, width, height);
    m_dropBars_A.emplace_back("test_2", 1, width, height);
    m_dropBars_A.emplace_back("test_3", 2, width, height);
}

void LeftPanel::draw()
{   
    ImGui::SetNextWindowPos(ImVec2{m_position.x, m_position.y});
    ImGui::SetNextWindowSize({m_width, m_height});

    ImGui::Begin("NODE PALETTE");

    Position pos = elementLocations::dropBarLocation_A;
    drawLeftPanels(pos, m_dropBars_A);
    
    ImGui::End();
}

void LeftPanel::drawLeftPanels(Position pos, std::vector<DropBar>& vectorGroupName)
{
    ImGui::SetCursorPos(ImVec2{pos.x, pos.y});
    
    for (auto& bar : vectorGroupName)
    {
        bar.draw();
    }
}