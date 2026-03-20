#include "mainEditor.h"
#include "graphState.h"
#include "graphSerializer.h"
#include "graphEditor.h"
#include "graphCompilation.h"
#include "imgui.h"
#include "../core/registry/nodeFactory.h"
#include "imgui_node_editor.h"

MainEditor::MainEditor(): m_fileBar(this),
    fileOpen(),
    fileNew(ImGuiFileBrowserFlags_EnterNewFilename)
{
    ed::Config config;
    config.SettingsFile = "node_editor.json";
    m_editorContext = ed::CreateEditor(&config);

    m_graphState = std::make_unique<GraphState>();
    m_graphEditor = std::make_unique<GraphEditor>(m_editorContext, *m_graphState);
    m_compiler = std::make_unique<GraphCompilation>();

    GraphSerializer::Load(*m_graphState, "graph.txt");
}

MainEditor::~MainEditor()
{
    // Set context as current before saving (needed for ed::GetNodePosition)
    ed::SetCurrentEditor(m_editorContext);
    
    // Save graph state to disk
    GraphSerializer::Save(*m_graphState, m_currentFilePath.c_str());
    
    // Clear current context
    ed::SetCurrentEditor(nullptr);
    
    // Clean up resources
    m_graphEditor.reset();
    ed::DestroyEditor(m_editorContext);
    m_editorContext = nullptr;
}

void MainEditor::draw()
{
    // Top toolbar
    if (ImGui::Button("Compile"))
    {
        m_compiler->CompileGraph(*m_graphState);
    }

    ImGui::SameLine();

    if (ImGui::Button("Clear"))
    {
        m_graphState->Clear();
        m_graphState->MarkDirty();
    }

    ImGui::SameLine();


    // Display compile status
    const std::string& compileMsg = m_graphState->GetCompileMessage();
    if (!compileMsg.empty())
    {
        ImVec4 col = m_graphState->IsCompileSuccess()
            ? ImVec4(0.2f, 0.9f, 0.2f, 1.0f)
            : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(compileMsg.c_str());
        ImGui::PopStyleColor();
    }

    else
    {
        ImGui::TextUnformatted(" ");
    }

    ImGui::Separator();
    // Draw the node editor canvas
    m_graphEditor->Draw();

    if (m_graphState->IsDirty())
    {
        ed::SetCurrentEditor(m_editorContext);
        GraphSerializer::Save(*m_graphState, m_currentFilePath.c_str());
        ed::SetCurrentEditor(nullptr);
        m_graphState->ClearDirty();
    }

    //For displaying file browser
    fileNew.Display();
    fileOpen.Display();

    //For selecting file to open
    if(fileOpen.HasSelected())
    {
        std::filesystem::path selectedPath = fileOpen.GetSelected();
        m_graphState=std::make_unique<GraphState>();
        m_graphEditor=std::make_unique<GraphEditor>(m_editorContext, *m_graphState);
        m_compiler=std::make_unique<GraphCompilation>();
        GraphSerializer::Load(*m_graphState, selectedPath.string().c_str());
        
        //Storing the opened file path for saving
        m_currentFilePath = selectedPath.string();
        fileOpen.ClearSelected();
    }
    if(fileNew.HasSelected())
    {
        std::filesystem::path selectedPath = fileNew.GetSelected();
        
        //Save current graph before creating new one
        ed::SetCurrentEditor(m_editorContext);
        GraphSerializer::Save(*m_graphState, m_currentFilePath.c_str());
        ed::SetCurrentEditor(nullptr);
        
        //Create fresh editor and load selected file
        m_graphEditor.reset();
        ed::DestroyEditor(m_editorContext);
        
        ed::Config config;
        config.SettingsFile = "node_editor.json";
        m_editorContext = ed::CreateEditor(&config);
        
        m_graphState = std::make_unique<GraphState>();
        m_graphEditor = std::make_unique<GraphEditor>(m_editorContext, *m_graphState);
        
        GraphSerializer::Load(*m_graphState, selectedPath.string().c_str());
        m_currentFilePath = selectedPath.string();
        fileNew.ClearSelected();
    }
}

//Create a new blank graph with fresh editor context
void MainEditor::NewGraph()
{
    //Save current graph before creating new one
    ed::SetCurrentEditor(m_editorContext);
    GraphSerializer::Save(*m_graphState, m_currentFilePath.c_str());
    ed::SetCurrentEditor(nullptr);
    
    //Destroy old editor context
    m_graphEditor.reset();
    ed::DestroyEditor(m_editorContext);
    
    //Create fresh editor context
    ed::Config config;
    config.SettingsFile = "node_editor.json";
    m_editorContext = ed::CreateEditor(&config);

    m_graphState = std::make_unique<GraphState>();
    m_graphEditor = std::make_unique<GraphEditor>(m_editorContext, *m_graphState);
    
    m_currentFilePath = "";
}

void MainEditor::OpenGraph()
{
    fileOpen.Open();
}