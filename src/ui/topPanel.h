/**
 * @brief Top toolbar panel for the editor
 * Displays application title and status information.
 */

#ifndef TOPPANEL_H
#define TOPPANEL_H

#include "../app/constants.h"
#include <vector>

/**
 * @brief Top toolbar panel
 * Fixed-height toolbar at the top of the application window.
 */
class TopPanel
{
public:
    /// Initialize the top panel
    TopPanel();
    
    /// Render the top panel (called every frame).
    void draw();
    
private:
    /// Height of the toolbar in pixels
    float m_height;
};

#endif
