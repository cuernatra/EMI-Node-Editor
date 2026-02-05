#ifndef LEFTPANEL_H
#define LEFTPANEL_H

#include "constants.h"
#include <vector>

/**
 * @brief Represents the panel which contains all the 
 * usable nodes.
 *
 * @author Atte Perkiö
 */
class LeftPanel
{
public: 
    LeftPanel();
    void draw();
    const float getWidth() const;
    void setWidth(float width);
    
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
