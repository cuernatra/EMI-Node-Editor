/**
 * @file ui.h
 * @brief Main UI compositor that manages the editor layout
 * 
 * Coordinates the top toolbar, left node palette panel, main editor canvas,
 * and resizable splitter. Creates a three-panel layout with adjustable sizes.
 * 
 * @author Atte Perkiö
 */

#ifndef UI_H
#define UI_H

#include "../editor/mainEditor.h"
#include "../ui/leftPanel.h"
#include "../ui/topPanel.h"
#include "../ui/consolePanel.h"
#include "../editor/settings.h"
#include <cstdint>

/**
 * @brief Main UI compositor class
 * 
 * Orchestrates the layout of all major UI components: top toolbar, left palette panel,
 * and main editor canvas. Provides a resizable splitter between the left panel and
 * main editor for user customization.
 * 
 * Layout structure:
 * - Top: Toolbar with compile/save/load buttons
 * - Left: Node palette with draggable node types
 * - Center: Main node editor canvas
 * 
 * @author Atte Perkiö
 */
class Ui
{
public:
    /**
     * @brief Initialize the UI compositor
     */
    Ui();
    
    /**
     * @brief Draw the complete UI hierarchy
     * 
     * Renders the top toolbar, left panel, splitter, and main editor.
     * Must be called every frame within the ImGui context.
     */
    void draw();
    
private:
    /**
     * @brief Draw an interactive splitter between left panel and editor
     * @param totalWidth Total available horizontal space
     * @param thickness Width of the splitter in pixels
     * @param minLeft Minimum width for left panel
     * @param minRight Minimum width for main editor
     * 
     * Creates a draggable vertical divider that allows users to resize
     * the left panel. Changes cursor to resize icon on hover.
     */
    void DrawSplitter(float totalWidth, float thickness, float minLeft, float minRight);

    /// Main node graph editor
    MainEditor m_mainEditor;
    
    /// Left sidebar with node palette
    LeftPanel m_leftPanel;
    
    /// Top toolbar with editor actions
    TopPanel m_topPanel;

    //bottom console panel
    ConsolePanel m_consolePanel;
    
    /// Current width of left panel (-1 = uninitialized, auto-sized on first draw)
    float m_leftPanelWidth = Settings::leftPanelWidth;
    
    /// Reserved for future right panel (currently unused)
    float m_rightPanelWidth = -1.f;

    /// Monotonic counter for unique overlay ids (forces fresh topmost window on selection change)
    uint32_t m_inspectorWindowGeneration = 0;

    /// Last single-selected node id used to detect selection transitions
    uintptr_t m_lastInspectorNodeId = 0;
};

#endif