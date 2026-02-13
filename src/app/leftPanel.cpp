#include "leftPanel.h"

LeftPanel::LeftPanel() 
{
    float width = elementSizes::dropBarWidth;
    float height = elementSizes::dropBarHeight;

    m_dropBars_A.emplace_back("test_1", 0, width, height, true);
    m_dropBars_A.emplace_back("test_2", 1, width, height);
    m_dropBars_A.emplace_back("test_3", 2, width, height);
}

void LeftPanel::draw()
{   
    ImGui::Text("NODE PALETTE");
    ImGui::Separator();
    drawNodeGroups(m_dropBars_A);
}

void LeftPanel::drawNodeGroups(std::vector<DropBar>& vectorGroupName)
{
    for(auto& bar : vectorGroupName)
    {
        bar.draw();
    }
}