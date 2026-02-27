/**
 * @file leftPanel.h
 * @brief Left sidebar node palette panel
 * 
 * Provides a scrollable list of node types that can be dragged into the
 * main editor canvas. Organizes nodes into collapsible categories.
 * 
 */

#ifndef LEFTPANEL_H
#define LEFTPANEL_H

#include "../app/constants.h"
#include "dropBar.h"
#include <vector>

/**
 * @brief Left sidebar containing the draggable node palette
 * 
 * Displays a vertical list of DropBar entries, each representing a node type
 * that can be instantiated in the editor. Users drag entries from this panel
 * onto the canvas to create new nodes.
 * 
 * The panel width is resizable via the main UI splitter.
 * 
 */
class LeftPanel
{
public:
    /**
     * @brief Initialize the left panel and populate node categories
     */
    LeftPanel();
    
    /**
     * @brief Render the left panel contents
     * 
     * Draws all DropBar entries in the palette. Must be called within
     * an ImGui child window context.
     */
    void draw();

private:
    /// Collection of collapsible node type categories
    std::vector<DropBar> m_dropBars;
};

#endif
