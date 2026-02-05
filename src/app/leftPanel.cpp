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
    ImGui::Text("NODE PALETTE");

<<<<<<< HEAD
    ImGui::Separator();

    if(ImGui::TreeNode("Control"))
    {
        for(int i = 0; i < 5; i++)
        {
            ImGui::Text("Some content nodes %d", i);
        }
        ImGui::TreePop();
    }

    if(ImGui::TreeNode("Data"))
    {
        for(int i = 0; i < 5; i++)
        {
            ImGui::Text("Some data nodes %d", i);
        }
        ImGui::TreePop();
    }

    if(ImGui::TreeNode("IO"))
    {
        for(int i = 0; i < 5; i++)
        {
            ImGui::Text("Some IO nodes %d", i);
        }
        ImGui::TreePop();
    }
    
}

const float LeftPanel::getWidth() const
{
    return m_width;
}

void LeftPanel::setWidth(float width)
{
    m_width = width;
=======
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
>>>>>>> d985bd6935f89068094f0ced9ac3acb1fc82d5c0
}