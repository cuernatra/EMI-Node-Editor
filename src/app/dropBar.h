#ifndef DROPBAR_H
#define DROPBAR_H

#include "constants.h"
#include <vector>
#include <string>

/**
 * @brief Represent drop bar
 *
 * @author Joel Turkka
 */
class DropBar
{
public: 
    DropBar(std::string name, int id, float width, float height);
    void draw();
    
private:
    std::string m_name;
    int m_id;
    float m_width;
    float m_height;
    bool m_open;
};

#endif
