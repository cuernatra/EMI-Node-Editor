/**
 * @brief Top-level editor controller.
 * Handles graph state, drawing, save/load, and compile flow.
 */

#pragma once

#include "../ui/fileBar.h"
#include "graph/graphState.h"
#include "graph/graphEditor.h"
#include "graph/graphCompilation.h"
#include <memory>
#include "../dependencies/imgui-filebrowser/imfilebrowser.h"
#include <cstdint>
#include <functional>
#include <string>
#include <future>
#include <mutex>
#include <vector>

namespace ed = ax::NodeEditor;

/**
 * @brief Main editor object used by the app loop.
 * Owns the node-editor context and graph subsystems.
 */
class MainEditor
{
public:
    /// Build editor state and load saved graph if present.
    MainEditor();
    
    /// Save state and release editor resources.
    ~MainEditor();

    /// Draw editor UI and graph canvas each frame.
    void draw();

    void NewGraph();   ///< Clear current graph and start a new one
    void OpenGraph();  ///< Load graph from file
    void SaveGraph();  ///< Save current graph to file
    void Exit();       ///< Save, clear, and exit the editor
    /// Render inspector contents for selected node.
    void drawInspectorPanel();

    /// Handle shared shortcuts even when overlay windows have focus.
    void handleSharedShortcuts();

    /// Return true when at least one alive node is selected.
    bool hasSelectedNode() const;

    /// Try to get currently selected node id (only when exactly one alive node is selected).
    bool tryGetSingleSelectedNodeId(uintptr_t& outId) const;

    /// Return true when graph has a Start node.
    bool hasStartNode() const;

    /// Return true when graph has at least one Variable node.
    bool hasVariables() const;

    /// Set receiver for compile log lines.
    void setCompileLogSink(std::function<void(const std::string&)> sink);

    /// Set callback invoked when Compile is triggered.
    void setCompileCallback(std::function<void()> cb);

    /// Read-only access for other editor panels.
    const GraphState& getGraphState() const;

    /// Copy live node positions from node-editor into graph state.
    void syncNodePositionsForPreview();

    void flushPendingCompileLogs();
    void pollAsyncCompileResult();

private:

    FileBar m_fileBar;                             ///< Top file menu bar
    ImGui::FileBrowser fileOpen;                   ///< File dialog for open action
    ImGui::FileBrowser fileSaveAs;                 ///< File dialog for save as action
    std::string m_currentFilePath = "";   ///< Path to the currently open file
    ed::EditorContext* m_editorContext = nullptr;  ///< imgui-node-editor context.
    std::unique_ptr<GraphState> m_graphState;      ///< Graph data.
    std::unique_ptr<GraphEditor> m_graphEditor;    ///< Graph canvas renderer.
    std::function<void(const std::string&)> m_uiCompileLogSink;
    std::mutex m_pendingCompileLogsMutex;
    std::vector<std::string> m_pendingCompileLogs;
    std::future<GraphCompilation::CompileResult> m_compileFuture;
    bool m_compileInProgress = false;
    std::unique_ptr<GraphCompilation> m_compiler;  ///< Graph compile/execute engine.
    std::function<void()> m_compileCallback;
    bool m_resultOnlyCompile = true;               ///< If true, keep compile status output minimal.
    bool m_shouldFocusAfterLoad = false;           ///< Flag to defer focus until next frame
};
