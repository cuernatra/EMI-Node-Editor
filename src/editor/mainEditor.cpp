#include "mainEditor.h"
#include "graphState.h"
#include "graphSerializer.h"
#include "graphEditor.h"
#include "graphCompilation.h"
#include "imgui.h"
#include "../core/registry/nodeFactory.h"
#include "imgui_node_editor.h"
#include "settings.h"
#include "../demo/AStarFinder/extra/astarDemoGraph.h"
#include <fstream>

MainEditor::MainEditor()
{
    ed::Config config;
    config.SettingsFile = "node_editor.json";
    m_editorContext = ed::CreateEditor(&config);

    m_graphState = std::make_unique<GraphState>();
    m_graphEditor = std::make_unique<GraphEditor>(m_editorContext, *m_graphState);
    m_compiler = std::make_unique<GraphCompilation>();

    GraphSerializer::Load(*m_graphState, "graph.txt");

    // load settings
    Settings::Load();
}

MainEditor::~MainEditor()
{
    // save settings
    Settings::Save();

    // Set context as current before saving (needed for ed::GetNodePosition)
    ed::SetCurrentEditor(m_editorContext);
    
    // Save graph state to disk
    GraphSerializer::Save(*m_graphState, "graph.txt");
    
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

    if (ImGui::Button("Load A* Demo"))
    {
        // Write the pre-built demo graph to disk then reload it.
        const std::string demoText = AStarDemo::Generate();
        {
            std::ofstream f("graph.txt", std::ios::trunc);
            f << demoText;
        }

        ed::SetCurrentEditor(m_editorContext);
        GraphSerializer::Load(*m_graphState, "graph.txt");

        // Force all freshly loaded demo nodes to apply their stored positions
        // (initialPos) on the next draw iteration.
        for (auto& n : m_graphState->GetNodes())
            n.positioned = false;

        ed::SetCurrentEditor(nullptr);
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
        GraphSerializer::Save(*m_graphState, "graph.txt");
        ed::SetCurrentEditor(nullptr);
        m_graphState->ClearDirty();
    }
}

void MainEditor::drawInspectorPanel()
{
    ed::SetCurrentEditor(m_editorContext);
    m_graphEditor->DrawInspectorPanel();
    ed::SetCurrentEditor(nullptr);
}

bool MainEditor::hasSelectedNode() const
{
    ed::SetCurrentEditor(m_editorContext);
    const bool selected = m_graphEditor->HasSelectedNode();
    ed::SetCurrentEditor(nullptr);
    return selected;
}
