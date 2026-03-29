#include "mainEditor.h"
#include "graphState.h"
#include "graphSerializer.h"
#include "graphEditor.h"
#include "graphCompilation.h"
#include "imgui.h"
#include "../core/registry/nodeFactory.h"
#include "imgui_node_editor.h"
#include "../core/graph/pin.h"
#include "settings.h"
#include "../ui/theme.h"
#include <chrono>
#include <utility>

MainEditor::MainEditor()
{
    ed::Config config;
    config.SettingsFile = "node_editor.json";
    config.CanvasSizeMode = ed::CanvasSizeMode::CenterOnly;
    m_editorContext = ed::CreateEditor(&config);

    // Apply editor pin shape settings from pin module.
    ed::SetCurrentEditor(m_editorContext);
    ApplyEditorPinStyle(ed::GetStyle());
    ed::SetCurrentEditor(nullptr);

    m_graphState = std::make_unique<GraphState>();
    m_graphEditor = std::make_unique<GraphEditor>(m_editorContext, *m_graphState);
    m_compiler = std::make_unique<GraphCompilation>();
    m_compiler->SetLogSink([this](const std::string& message)
    {
        std::lock_guard<std::mutex> lock(m_pendingCompileLogsMutex);
        m_pendingCompileLogs.push_back(message);
    });

    GraphSerializer::Load(*m_graphState, "graph.txt");

    // load settings
    Settings::Load();
}

MainEditor::~MainEditor()
{
    if (m_compileFuture.valid())
    {
        m_compileFuture.wait();
        m_compileInProgress = false;
    }

    flushPendingCompileLogs();

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
    flushPendingCompileLogs();
    pollAsyncCompileResult();

    // Top toolbar on a single row:
    // [Compile] [Result only] [Clear] [compile status message..........] [+]
    if (ImGui::Button("Compile"))
    {
        if (m_compileInProgress)
        {
            if (m_uiCompileLogSink)
                m_uiCompileLogSink("[WARN] Compile already running...\n");
        }
        else
        {
        // Notify listeners immediately on click (before potentially long compile/execute),
        // so preview window can open right away.
        if (m_compileCallback)
            m_compileCallback();

            m_graphState->SetCompileStatus(false, "[INFO] Compiling graph...\n");

            const std::vector<VisualNode> nodesSnapshot = m_graphState->GetNodes();
            const std::vector<Link> linksSnapshot = m_graphState->GetLinks();
            const bool resultOnlySnapshot = m_resultOnlyCompile;
            GraphCompilation* compiler = m_compiler.get();

            m_compileInProgress = true;
            m_compileFuture = std::async(std::launch::async,
                [compiler, nodesSnapshot, linksSnapshot, resultOnlySnapshot]() mutable
                {
                    return compiler->CompileGraphSnapshot(nodesSnapshot, linksSnapshot, resultOnlySnapshot);
                });
        }
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
            ? colors::green
            : colors::error;
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
    m_uiCompileLogSink = std::move(sink);
}

void MainEditor::setCompileCallback(std::function<void()> cb)
{
    m_compileCallback = std::move(cb);
}

const GraphState& MainEditor::getGraphState() const
{
    return *m_graphState;
}

void MainEditor::syncNodePositionsForPreview()
{
    ed::SetCurrentEditor(m_editorContext);
    for (auto& node : m_graphState->GetNodes())
    {
        if (!node.alive)
            continue;

        node.initialPos = ed::GetNodePosition(node.id);
        node.positioned = true;
    }
    ed::SetCurrentEditor(nullptr);
}

void MainEditor::flushPendingCompileLogs()
{
    if (!m_uiCompileLogSink)
        return;

    std::vector<std::string> logs;
    {
        std::lock_guard<std::mutex> lock(m_pendingCompileLogsMutex);
        if (m_pendingCompileLogs.empty())
            return;

        logs.swap(m_pendingCompileLogs);
    }

    for (const auto& msg : logs)
        m_uiCompileLogSink(msg);
}

void MainEditor::pollAsyncCompileResult()
{
    if (!m_compileInProgress || !m_compileFuture.valid())
        return;

    using namespace std::chrono_literals;
    if (m_compileFuture.wait_for(0ms) != std::future_status::ready)
        return;

    const GraphCompilation::CompileResult outcome = m_compileFuture.get();
    m_compileInProgress = false;
    m_graphState->SetCompileStatus(outcome.success, outcome.message);
}
