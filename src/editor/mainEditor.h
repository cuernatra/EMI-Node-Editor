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
#include <functional>
#include <string>
#include <future>
#include <mutex>
#include <vector>

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

    /// Handle shared shortcuts (e.g. Delete) even when overlay windows are focused.
    void handleSharedShortcuts();

    /// Check whether any existing node is currently selected.
    bool hasSelectedNode() const;

    /// Try to get currently selected node id (only when exactly one alive node is selected).
    bool tryGetSingleSelectedNodeId(uintptr_t& outId) const;

    /// Check whether graph currently contains a Start node.
    bool hasStartNode() const;

    /// Check whether graph contains at least one Variable node.
    bool hasVariables() const;

    /// Forward compile status messages to an external log sink.
    void setCompileLogSink(std::function<void(const std::string&)> sink);

    /// Set callback invoked after Compile button runs compilation.
    void setCompileCallback(std::function<void()> cb);

    /// Expose graph state for read-only auxiliary views like preview panels.
    const GraphState& getGraphState() const;

    /// Sync live node positions from node editor so previews can use current layout.
    void syncNodePositionsForPreview();

    void flushPendingCompileLogs();
    void pollAsyncCompileResult();

private:

    ed::EditorContext* m_editorContext = nullptr;  ///< imgui-node-editor context
    std::unique_ptr<GraphState> m_graphState;      ///< Graph data (must be constructed before GraphEditor)
    std::unique_ptr<GraphEditor> m_graphEditor;    ///< Canvas renderer (holds reference to m_graphState)
    std::function<void(const std::string&)> m_uiCompileLogSink;
    std::mutex m_pendingCompileLogsMutex;
    std::vector<std::string> m_pendingCompileLogs;
    std::future<GraphCompilation::CompileResult> m_compileFuture;
    bool m_compileInProgress = false;
    std::unique_ptr<GraphCompilation> m_compiler;  ///< Graph compilation and execution engine
    std::function<void()> m_compileCallback;
    bool m_resultOnlyCompile = true;               ///< If true, compile status stays minimal while terminal prints result
};
