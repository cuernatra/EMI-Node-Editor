/**
 * @brief Main editor controller
 * Coordinates graph state, rendering, serialization, and compilation.
 */

#pragma once

#include "graphState.h"
#include "graphEditor.h"
#include "graphCompilation.h"
#include "../ui/fileBar.h"
#include <memory>
#include "../dependencies/imgui-filebrowser/imfilebrowser.h"

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

    void NewGraph();   ///< Clear current graph and start a new one
    void OpenGraph();  ///< Load graph from file
    void SaveGraph();  ///< Save current graph to file
    void Exit();       ///< Save, clear, and exit the editor

private:

    ed::EditorContext* m_editorContext = nullptr;  ///< imgui-node-editor context
    std::unique_ptr<GraphState> m_graphState;      ///< Graph data (must be constructed before GraphEditor)
    std::unique_ptr<GraphEditor> m_graphEditor;    ///< Canvas renderer (holds reference to m_graphState)
    std::unique_ptr<GraphCompilation> m_compiler;  ///< Graph compilation and execution engine

    FileBar m_fileBar;                             ///< Top file menu bar
    ImGui::FileBrowser fileOpen;                   ///< File dialog for open action
    ImGui::FileBrowser fileSaveAs;                 ///< File dialog for save as action
    std::string m_currentFilePath = "";   ///< Path to the currently open file
};
