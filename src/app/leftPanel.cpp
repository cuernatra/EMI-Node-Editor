#include "leftPanel.h"

LeftPanel::LeftPanel() 
    : m_width{appConstants::windowWidth / 5}
    , m_height{appConstants::windowheight}
    , m_position{0,0}
    , m_open{true}
{
}

void LeftPanel::draw()
{   
    ImGui::Text("NODE PALETTE");

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
}