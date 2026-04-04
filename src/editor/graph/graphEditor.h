/** @file graphEditor.h */
/** @brief Canvas-side graph editor interactions and rendering. */

#pragma once

#include "core/graph/visualNode.h"
#include "imgui_node_editor.h"
#include <cstdint>

class GraphState;

namespace ed = ax::NodeEditor;

/** @brief Draws graph canvas and applies editor actions to GraphState. */
class GraphEditor
{
public:
    /** @brief Construct editor from node-editor context and graph state. */
    GraphEditor(ed::EditorContext* ctx, GraphState& state);
    
    /** @brief Destroy editor resources. */
    ~GraphEditor();

    /** @brief Draw the canvas and process one frame of interactions. */
    void Draw();

    /** @brief Draw inspector content for current selection. */
    void DrawInspectorPanel();

    /** @brief Return true when exactly one alive node is selected. */
    bool HasSelectedNode() const;

    /** @brief Apply delete/backspace fallback for current selection. */
    void HandleDeleteShortcutFallback();

    /** @brief Read selected node id when a single alive node is selected. */
    bool TryGetSingleSelectedNodeId(uintptr_t& outId) const;

private:
    /** @brief Draw nodes, links, and canvas-level interactions. */
    void DrawNodeCanvas();

    /** @brief Validate and commit a newly dragged link. */
    void CreateNewLink();
    
    /** @brief Delete accepted node requests from node-editor API. */
    void DeleteNodes(ed::NodeId nodeId);
    
    /** @brief Delete accepted link requests from node-editor API. */
    void DeleteLinks(ed::LinkId linkId);

    void RefreshGraphAndMarkDirty();


    ed::EditorContext* m_ctx;  ///< The imgui-node-editor context
    GraphState& m_state;       ///< Reference to the graph state
    bool m_initialAutoStartHandled = false; ///< Ensures empty-graph Start auto-spawn runs only once at startup
};
