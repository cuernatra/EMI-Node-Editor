/**
 * @file dropBar.h
 * @brief Collapsible draggable node entry in the palette
 * 
 * Defines individual draggable items in the left panel node palette.
 * Each DropBar represents a specific node type and handles drag-and-drop
 * spawning of that node type into the editor canvas.
 * 
 * @author Atte Perkiö
 */

#ifndef DROPBAR_H
#define DROPBAR_H

#include "../app/constants.h"
#include "../core/graph/types.h"         // NodeType
#include "../core/graph/link.h"          // NodeSpawnPayload
#include <vector>
#include <string>

/**
 * @brief Collapsible, draggable node type entry in the palette
 * 
 * Each DropBar represents one node type (e.g., "Add", "Branch", "Output").
 * Users can drag from this entry to spawn a new node of that type in the editor.
 * The associated NodeType is embedded in the drag-and-drop payload for proper
 * node instantiation.
 * 
 */
class DropBar
{
public:
    /**
     * @brief Construct a node palette entry
     * @param label Display name shown in the UI
     * @param id Unique identifier for this palette entry
     * @param nodeType The type of node this entry creates
     * @param width Display width in pixels
     * @param height Display height in pixels
     * @param hasSpawnButton Whether to show a dedicated spawn button (optional)
     */
    DropBar(std::string label, int id, NodeType nodeType,
            float width, float height, bool hasSpawnButton);

    /**
     * @brief Render this drop bar entry
     * 
     * Draws the collapsible header and handles drag-and-drop source logic.
     * Creates a NodeSpawnPayload containing the associated NodeType.
     */
    void draw();

private:
    std::string m_label;          ///< Display name shown in the header
    int         m_id;             ///< Unique ID for ImGui
    NodeType    m_nodeType;       ///< The node type this bar spawns
    float       m_width;          ///< Display width in pixels
    float       m_height;         ///< Display height in pixels
    bool        m_open;           ///< Whether the collapsible section is expanded
    bool        m_hasSpawnButton; ///< Show dedicated spawn button (optional)
};

#endif
