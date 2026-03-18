/**
 * @brief Main editor controller
 * Coordinates graph state, rendering, serialization, and compilation.
 */

#pragma once

#include "graphState.h"
#include "graphEditor.h"
#include "graphCompilation.h"
#include <memory>
#include <cstdint>

namespace ed = ax::NodeEditor;

/**
 * @brief Main editor orchestrator
 * Manages graph state, canvas rendering, serialization, and compilation.
 * Owns the node editor context.
 */
class MainEditor
{
public:
    /// Initialize editor; loads saved graph if exists.
    MainEditor();
    
    /// Clean up and save current graph state.
    ~MainEditor();

    /// Render editor UI and canvas (called every frame).
    void draw();

    /// Render inspector contents for selected node.
    void drawInspectorPanel();

    /// Check whether any existing node is currently selected.
    bool hasSelectedNode() const;

    /// Try to get currently selected node id (only when exactly one alive node is selected).
    bool tryGetSingleSelectedNodeId(uintptr_t& outId) const;

    /// Check whether graph currently contains a Start node.
    bool hasStartNode() const;

    /// Check whether graph contains at least one Variable node.
    bool hasVariables() const;

private:

    ed::EditorContext* m_editorContext = nullptr;  ///< imgui-node-editor context
    std::unique_ptr<GraphState> m_graphState;      ///< Graph data (must be constructed before GraphEditor)
    std::unique_ptr<GraphEditor> m_graphEditor;    ///< Canvas renderer (holds reference to m_graphState)
    std::unique_ptr<GraphCompilation> m_compiler;  ///< Graph compilation and execution engine
    bool m_resultOnlyCompile = true;               ///< If true, compile status stays minimal while terminal prints result
};
