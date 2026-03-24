#include "mainEditor.h"
#include "graphState.h"
#include "graphSerializer.h"
#include "graphEditor.h"
#include "graphCompilation.h"
#include "imgui.h"
#include "../core/registry/nodeFactory.h"
#include "imgui_node_editor.h"
#include "settings.h"
#include <utility>

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
    // Top toolbar on a single row:
    // [Compile] [Result only] [Clear] [compile status message..........] [+]
    if (ImGui::Button("Compile"))
    {
        m_compiler->CompileGraph(*m_graphState, m_resultOnlyCompile);
    }

    ImGui::SameLine();
    ImGui::Checkbox("Result only", &m_resultOnlyCompile);

    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        m_graphState->Clear();
        m_graphState->MarkDirty();
    }

    ImGui::SameLine();

    const float plusButtonWidth = ImGui::CalcTextSize("Focus for nodes").x + ImGui::GetStyle().FramePadding.x * 2.0f + 5.0f;
    const float statusSpacing = ImGui::GetStyle().ItemSpacing.x;
    float statusWidth = ImGui::GetContentRegionAvail().x - plusButtonWidth - statusSpacing;
    if (statusWidth < 80.0f)
        statusWidth = 80.0f;

    ImGui::BeginChild("##CompileStatusArea", ImVec2(statusWidth, ImGui::GetFrameHeight()), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
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
    ImGui::EndChild();

    ImGui::SameLine();
    if (ImGui::Button("Focus for nodes"))
    {
        ed::SetCurrentEditor(m_editorContext);
        if (m_graphState->HasAliveNodes())
            ed::NavigateToContent();
        ed::SetCurrentEditor(nullptr);
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

void MainEditor::handleSharedShortcuts()
{
    ed::SetCurrentEditor(m_editorContext);
    m_graphEditor->HandleDeleteShortcutFallback();
    ed::SetCurrentEditor(nullptr);
}

bool MainEditor::hasSelectedNode() const
{
    ed::SetCurrentEditor(m_editorContext);
    const bool selected = m_graphEditor->HasSelectedNode();
    ed::SetCurrentEditor(nullptr);
    return selected;
}

bool MainEditor::tryGetSingleSelectedNodeId(uintptr_t& outId) const
{
    ed::SetCurrentEditor(m_editorContext);
    const bool hasOne = m_graphEditor->TryGetSingleSelectedNodeId(outId);
    ed::SetCurrentEditor(nullptr);
    return hasOne;
}

bool MainEditor::hasStartNode() const
{
    return m_graphState->HasNodeType(NodeType::Start);
}

bool MainEditor::hasVariables() const
{
    return m_graphState->HasNodeType(NodeType::Variable);
}

void MainEditor::setCompileLogSink(std::function<void(const std::string&)> sink)
{
    if (m_compiler)
    {
        m_compiler->SetLogSink(std::move(sink));
    }
}
