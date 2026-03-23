/**
 * @brief Console panel for editor output and logs
 * Display area for compiler messages, runtime logs, and other output.
 */
#ifndef CONSOLEPANEL_H
#define CONSOLEPANEL_H

#include "../app/constants.h"
#include <vector>

/**
 * @brief Console panel class
 * Manages the display and functionality of the console output area.
 */
class ConsolePanel
{
public:
    /// Initialize the console panel
    ConsolePanel();
    
    /// Render the console panel (called every frame).
    void draw();
    
private:
    /// Height of the console panel in pixels
    float m_height;
};

#endif