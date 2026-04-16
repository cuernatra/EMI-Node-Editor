#include "mainEditor.h"
#include "graph/graphState.h"
#include "graph/graphSerializer.h"
#include "graph/graphEditor.h"
#include "graph/graphExecutor.h"
#include "imgui.h"
#include "../core/registry/nodeFactory.h"
#include "imgui_node_editor.h"
#include "renderer/pinRenderer.h"
#include "settings.h"
#include "../ui/theme.h"
#include <chrono>
#include <utility>

MainEditor::MainEditor(): m_fileBar(this),
    fileOpen(ImGuiFileBrowserFlags_ConfirmOnEnter),
    fileSaveAs(ImGuiFileBrowserFlags_EnterNewFilename)
{
    ed::Config config;
    //config.SettingsFile = "node_editor.json";
    config.CanvasSizeMode = ed::CanvasSizeMode::CenterOnly;
    m_editorContext = ed::CreateEditor(&config);

    // Apply pin style settings from pin renderer module.
    ed::SetCurrentEditor(m_editorContext);
    ApplyEditorPinStyle(ed::GetStyle());
    ed::SetCurrentEditor(nullptr);

    m_graphState = std::make_unique<GraphState>();
    m_graphEditor = std::make_unique<GraphEditor>(m_editorContext, *m_graphState);
    m_compiler = std::make_unique<GraphExecutor>();
    m_compiler->SetLogSink([this](const std::string& message)
    {
        std::lock_guard<std::mutex> lock(m_pendingCompileLogsMutex);
        m_pendingCompileLogs.push_back(message);
    });

    GraphSerializer::Load(*m_graphState, "graph.txt");
    m_currentFilePath = "graph.txt";

    // Load persisted UI/theme settings.
    Settings::Load();

    fileOpen.SetTypeFilters({".txt"});
}

MainEditor::~MainEditor()
{

    // Ensure any running compilation is force-stopped before waiting, with timeout.
    if (m_compileFuture.valid())
    {
        if (m_compiler)
            m_compiler->RequestForceStop();
        using namespace std::chrono_literals;
        if (m_compileFuture.wait_for(1s) != std::future_status::ready)
        {
            // Timed out waiting for compilation to finish; skip waiting to avoid hang.
            // The thread will be abandoned, but the process will exit cleanly.
        }
        else
        {
            m_compileFuture.get();
        }
        m_compileInProgress = false;
    }

    flushPendingCompileLogs();

    // Save UI/theme settings.
    Settings::Save();

    // Set context before save so node positions can be queried correctly.
    ed::SetCurrentEditor(m_editorContext);
    
    // Save graph state to disk
    GraphSerializer::Save(*m_graphState, m_currentFilePath.c_str());
    
    /* Old save path
    // Save graph to disk.
    GraphSerializer::Save(*m_graphState, "graph.txt");
    */
    
    // Clear current editor context.
    ed::SetCurrentEditor(nullptr);
    
    // Release resources.
    m_graphEditor.reset();
    ed::DestroyEditor(m_editorContext);
    m_editorContext = nullptr;
}

void MainEditor::draw()
{
    flushPendingCompileLogs();
    pollAsyncCompileResult();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, colors::elevated);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colors::topPanelButtonHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors::surface);

    // Top toolbar row.
    if (ImGui::Button("Compile"))
    {
        if (m_compileInProgress)
        {
            if (m_uiCompileLogSink)
                m_uiCompileLogSink("[WARN] Compile already running...\n");
        }
        else
        {
        // Notify listeners immediately so preview UI can react right away.
        if (m_compileCallback)
            m_compileCallback();

            m_graphState->SetCompileStatus(false, "[INFO] Compiling graph...\n");

            const std::vector<VisualNode> nodesSnapshot = m_graphState->GetNodes();
            const std::vector<Link> linksSnapshot = m_graphState->GetLinks();
            const bool resultOnlySnapshot = m_resultOnlyCompile;
            GraphExecutor* compiler = m_compiler.get();

            if (m_compiler)
                m_compiler->ClearForceStopRequest();

            m_compileInProgress = true;
            m_compileFuture = std::async(std::launch::async,
                [compiler, nodesSnapshot, linksSnapshot, resultOnlySnapshot]() mutable
                {
                    return compiler->CompileGraphSnapshot(nodesSnapshot, linksSnapshot, resultOnlySnapshot);
                });
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Force Stop"))
    {
        if (m_compiler)
            m_compiler->RequestForceStop();
        m_graphState->SetCompileStatus(false, "[WARN] Force stop requested...\n");
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
    
    //Display current file name
    ImGui::SameLine();
    std::filesystem::path p(m_currentFilePath);
    std::string fileName = p.filename().string();
    std::string displayPath = fileName;
    ImGui::Text("| File: %s", displayPath.c_str());
    ImGui::EndChild();

    ImGui::SameLine();
    if (ImGui::Button("Focus for nodes"))
    {
        ed::SetCurrentEditor(m_editorContext);
        if (m_graphState->HasAliveNodes())
            ed::NavigateToContent();
        ed::SetCurrentEditor(nullptr);
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
    

    ImGui::Separator();

    ed::SetCurrentEditor(m_editorContext);
    ed::Style& style = ed::GetStyle();
    style.Colors[ed::StyleColor_Bg] = ImVec4(
        Settings::gridBgColorR,
        Settings::gridBgColorG,
        Settings::gridBgColorB,
        Settings::gridBgColorA
    );
    style.Colors[ed::StyleColor_Grid] = ImVec4(
        Settings::gridLineColorR,
        Settings::gridLineColorG,
        Settings::gridLineColorB,
        Settings::gridLineColorA
    );
    ed::SetCurrentEditor(nullptr);

    //Draw graph canvas.
    m_graphEditor->Draw();

    if (m_shouldFocusAfterLoad && m_graphState->HasAliveNodes())
    {
        ed::SetCurrentEditor(m_editorContext);
        ed::NavigateToContent(0.0f);
        ed::SetCurrentEditor(nullptr);
        m_shouldFocusAfterLoad = false;
    }

    if (m_graphState->IsDirty())
    {
        ed::SetCurrentEditor(m_editorContext);
        GraphSerializer::Save(*m_graphState, m_currentFilePath.c_str());
        ed::SetCurrentEditor(nullptr);
        m_graphState->ClearDirty();
    }

    //For displaying file browser
    fileOpen.Display();
    fileSaveAs.Display();

    //For selecting file to open
    if(fileOpen.HasSelected())
    {
        //Save current file before opening new file
        if (!m_currentFilePath.empty())
        {
            ed::SetCurrentEditor(m_editorContext);
            GraphSerializer::Save(*m_graphState, m_currentFilePath.c_str());
            ed::SetCurrentEditor(nullptr);
        }

        std::filesystem::path selectedPath = fileOpen.GetSelected();
        
        m_graphState=std::make_unique<GraphState>();
        m_graphEditor=std::make_unique<GraphEditor>(m_editorContext, *m_graphState);
        m_compiler=std::make_unique<GraphExecutor>();
        GraphSerializer::Load(*m_graphState, selectedPath.string().c_str());

        //Apply loaded positions to the editor
        ed::SetCurrentEditor(m_editorContext);
        for (const auto& n : m_graphState->GetNodes())
        {
            if (n.alive)
                ed::SetNodePosition(n.id, n.initialPos);
        }
        ed::SetCurrentEditor(nullptr);

        //Storing the opened file path for saving
        m_currentFilePath = selectedPath.string();
        fileOpen.ClearSelected();

        //Focus on nodes after loading
        m_shouldFocusAfterLoad = true;
    }
    if(fileSaveAs.HasSelected())
    {
        std::filesystem::path selectedPath = fileSaveAs.GetSelected();
        
        if (selectedPath.extension() != ".txt")
        {
            selectedPath.replace_extension(".txt");
        }
        
        //Save current graph to new file
        ed::SetCurrentEditor(m_editorContext);
        GraphSerializer::Save(*m_graphState, selectedPath.string().c_str());
        ed::SetCurrentEditor(nullptr);
        
        //Update current file path to the new location
        m_currentFilePath = selectedPath.string();
        m_graphState->ClearDirty();
        fileSaveAs.ClearSelected();
        fileSaveAs.Close();
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
    //config.SettingsFile = "node_editor.json";
    m_editorContext = ed::CreateEditor(&config);

    m_graphState = std::make_unique<GraphState>();
    m_graphEditor = std::make_unique<GraphEditor>(m_editorContext, *m_graphState);
    
    m_currentFilePath = "";
}

void MainEditor::OpenGraph()
{
    fileOpen.Open();
}

void MainEditor::SaveGraph()
{
    if (m_currentFilePath.empty())
    {
        fileSaveAs.Open();
    }
    else
    {
        ed::SetCurrentEditor(m_editorContext);
        GraphSerializer::Save(*m_graphState, m_currentFilePath.c_str());
        ed::SetCurrentEditor(nullptr);
        m_graphState->ClearDirty();
    }
}

void MainEditor::Exit()
{
    //Save current graph before exiting
    ed::SetCurrentEditor(m_editorContext);
    GraphSerializer::Save(*m_graphState, m_currentFilePath.c_str());
    ed::SetCurrentEditor(nullptr);
    
    //Clean up
    m_graphEditor.reset();
    ed::DestroyEditor(m_editorContext);
    m_editorContext = nullptr;

    exit(0);
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

    const GraphExecutor::CompileResult outcome = m_compileFuture.get();
    m_compileInProgress = false;
    m_graphState->SetCompileStatus(outcome.success, outcome.message);
}