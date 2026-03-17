/**
 * @brief Main editor controller
 * Coordinates graph state, rendering, serialization, and compilation.
 */

#pragma once

#include "graphState.h"
#include "graphEditor.h"
#include "graphCompilation.h"
#include <memory>

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

private:

    ed::EditorContext* m_editorContext = nullptr;  ///< imgui-node-editor context
    std::unique_ptr<GraphState> m_graphState;      ///< Graph data (must be constructed before GraphEditor)
    std::unique_ptr<GraphEditor> m_graphEditor;    ///< Canvas renderer (holds reference to m_graphState)
    std::unique_ptr<GraphCompilation> m_compiler;  ///< Graph compilation and execution engine
};
