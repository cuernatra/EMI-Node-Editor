#ifndef TOPPANEL_H
#define TOPPANEL_H

#include "constants.h"
#include <vector>

/**
 * @brief Represents the static panel,
 * which is always visible.
 *
 * @author Joel Turkka
 */
class TopPanel
{
public: 
    TopPanel();
    void draw();
    
private:
    float m_height;
};

#endif
