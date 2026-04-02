/**
 * @file graphEditor.h
 * @brief Node editor canvas renderer and interaction handler
 *
 * This class handles the visible editor side:
 * drawing the canvas, reading user input, and running editor actions.
 * Shared graph rules and helper code should stay in graphEditorUtils
 * so this file is easier to read and safer to change.
 * 
 * Manages the imgui-node-editor canvas: rendering nodes and links,
 * handling user interactions (dragging, connecting, deleting),
 * and node spawn via drag/drop.
 * 
 */

#pragma once

#include "core/graph/visualNode.h"
#include "imgui_node_editor.h"
#include <cstdint>

class GraphState;

namespace ed = ax::NodeEditor;

/**
 * @brief Node editor canvas renderer and interaction manager
 * 
 * Handles all visual and interactive aspects of the node graph:
 * - Drawing nodes with pins and fields
 * - Rendering links between pins
 * - Processing user interactions (drag, connect, delete)
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
    * editor actions.
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

    /**
     * @brief Handle shared delete shortcuts (Delete/Backspace) for current selection
     *
     * Intended to be called from a higher-level UI pass so overlay windows can
     * reuse canvas shortcuts even when they have focus.
     */
    void HandleDeleteShortcutFallback();

    /**
     * @brief Get selected node id when exactly one alive node is selected
     * @param outId Receives selected node id (raw uintptr value)
     * @return true when exactly one alive node is selected, otherwise false
     */
    bool TryGetSingleSelectedNodeId(uintptr_t& outId) const;

private:
    /**
     * @brief Draw all nodes and links on the canvas
     * 
     * Iterates through all nodes and links in the graph state,
     * rendering them using imgui-node-editor primitives.
     */
    void DrawNodeCanvas();

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

    void RefreshGraphAndMarkDirty();


    ed::EditorContext* m_ctx;  ///< The imgui-node-editor context
    GraphState& m_state;       ///< Reference to the graph state
    bool m_initialAutoStartHandled = false; ///< Ensures empty-graph Start auto-spawn runs only once at startup
};
