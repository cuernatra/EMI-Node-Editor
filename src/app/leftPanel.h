#ifndef LEFTPANEL_H
#define LEFTPANEL_H

#include "panel.h"
#include "constants.h"
#include <vector>

/**
 * @brief Represents the panel which contains all the 
 * usable nodes.
 *
 * @author Atte Perkiö
 */
class LeftPanel : public Panel
{
public: 
    LeftPanel();
    virtual void draw() override;
    
private:
    float m_width;
    float m_height;
    Position m_position;
    bool m_open;
    //std::vector<Node> m_controlFlowNodes;
    //std::vector<Node> m_dataNodes;
    //std::vector<Node> m_IONodes;
};

#endif
