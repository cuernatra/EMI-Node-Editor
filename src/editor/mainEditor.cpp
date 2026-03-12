#include "mainEditor.h"
#include "graphState.h"
#include "graphSerializer.h"
#include "graphEditor.h"
#include "graphCompilation.h"
#include "imgui.h"
#include "../core/registry/nodeFactory.h"
#include "imgui_node_editor.h"
#include <algorithm>

MainEditor::MainEditor()
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

    ImGui::Separator();

    const bool showInspector = m_graphEditor->HasSelectedNode();

    if (showInspector)
    {
        const float splitterWidth = 6.0f;
        const float minInspectorWidth = 240.0f;
        const float defaultInspectorWidth = 300.0f;
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        const float inspectorWidth = std::clamp(
            defaultInspectorWidth,
            minInspectorWidth,
            std::max(minInspectorWidth, avail.x - 180.0f));

        const float canvasWidth = std::max(100.0f, avail.x - inspectorWidth - splitterWidth);

        ImGui::BeginChild("GRAPH CANVAS", ImVec2(canvasWidth, 0), true);
        m_graphEditor->Draw();
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(splitterWidth, 0));
        ImGui::SameLine();

        ImGui::BeginChild("INSPECTOR PANEL", ImVec2(0, 0), true);
        m_graphEditor->DrawInspector();
        ImGui::EndChild();
    }
    else
    {
        ImGui::BeginChild("GRAPH CANVAS", ImVec2(0, 0), true);
        m_graphEditor->Draw();
        ImGui::EndChild();
    }

    if (m_graphState->IsDirty())
    {
        ed::SetCurrentEditor(m_editorContext);
        GraphSerializer::Save(*m_graphState, "graph.txt");
        ed::SetCurrentEditor(nullptr);
        m_graphState->ClearDirty();
    }
}
