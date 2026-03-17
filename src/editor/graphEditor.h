/**
 * @file graphEditor.h
 * @brief Node editor canvas renderer and interaction handler
 * 
 * Manages the imgui-node-editor canvas: rendering nodes and links,
 * handling user interactions (dragging, connecting, deleting),
 * and displaying context menus.
 * 
 */

#pragma once

#include "../core/graph/visualNode.h"
#include "imgui_node_editor.h"

class GraphState;

namespace ed = ax::NodeEditor;

/**
 * @brief Node editor canvas renderer and interaction manager
 * 
 * Handles all visual and interactive aspects of the node graph:
 * - Drawing nodes with pins and fields
 * - Rendering links between pins
 * - Processing user interactions (drag, connect, delete)
 * - Displaying context menus for spawning/removing elements
 * - Validating link creation (type compatibility, cycle detection)
 * 
 * Operates on a GraphState reference but doesn't own it.
 * 
 */
class GraphEditor
{
public:
    /**
     * @brief Initialize the graph editor
     * @param ctx The imgui-node-editor context to use for rendering
     * @param state Reference to the graph state to display and modify
     */
    GraphEditor(ed::EditorContext* ctx, GraphState& state);
    
    /**
     * @brief Clean up editor resources
     */
    ~GraphEditor();

    /**
     * @brief Draw the editor canvas and handle all interactions
     * 
     * Must be called within an ed::Begin() / ed::End() block.
     * Renders all nodes and links, processes user input, and handles
     * context menu display and actions.
     */
    void Draw();

    /**
     * @brief Draw inspector panel contents for selected node
     *
     * Intended to be called inside a host ImGui child/window.
     */
    void DrawInspectorPanel();

    /**
     * @brief Check if an existing node is currently selected
     */
    bool HasSelectedNode() const;

private:
    /**
     * @brief Draw all nodes and links on the canvas
     * 
     * Iterates through all nodes and links in the graph state,
     * rendering them using imgui-node-editor primitives.
     */
    void DrawNodeCanvas();
    
    /**
     * @brief Display and handle context menus
     * 
     * Shows right-click context menus for spawning new nodes,
     * creating links, and deleting elements.
     */
    void DrawContextMenus();

    /**
     * @brief Handle new link creation
     * 
     * Validates pin compatibility, checks for cycles, and creates
     * the link if valid. Called when user completes a connection drag.
     */
    void CreateNewLink();
    
    /**
     * @brief Delete a node and all its connected links
     * @param nodeId The node to delete
     */
    void DeleteNodes(ed::NodeId nodeId);
    
    /**
     * @brief Delete a single link
     * @param linkId The link to delete
     */
    void DeleteLinks(ed::LinkId linkId);

    ed::EditorContext* m_ctx;  ///< The imgui-node-editor context
    GraphState& m_state;       ///< Reference to the graph state
    bool m_initialAutoStartHandled = false; ///< Ensures empty-graph Start auto-spawn runs only once at startup
};
