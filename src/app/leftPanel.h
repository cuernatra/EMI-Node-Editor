#ifndef LEFTPANEL_H
#define LEFTPANEL_H

#include "constants.h"
#include "dropBar.h"
#include <vector>

/**
 * @brief Represents the panel which contains all the 
 * usable nodes.
 *
 * @author Atte Perkiö, Joel Turkka
 */
class LeftPanel
{
public: 
    LeftPanel();
    void draw();
    
private:
    float m_width;
    float m_height;
    Position m_position;
    bool m_open;

    std::vector<DropBar> m_dropBars_A;
    
    //std::vector<Node> m_controlFlowNodes;
    //std::vector<Node> m_dataNodes;
    //std::vector<Node> m_IONodes;

    void drawLeftPanels(Position pos, std::vector<DropBar>& vectorGroupName);
};


#endif
