#include "mainEditor.h"
#include "imgui.h"

namespace ed = ax::NodeEditor;

MainEditor::MainEditor()
{
    ed::Config config;
    config.SettingsFile = "node_editor.json";
    m_ctx = ed::CreateEditor(&config);
}

MainEditor::~MainEditor()
{
    ed::DestroyEditor(m_ctx);
    m_ctx = nullptr;
}

void MainEditor::draw()
{
    ImGui::Text("EMI-EDITOR");
    ImGui::Separator();

    ed::SetCurrentEditor(m_ctx);
    ed::Begin("MainGraph");

    if (m_firstFrame)
    {
        m_firstFrame = false;
        ed::SetNodePosition(ed::NodeId(1), ImVec2(60, 60));
        ed::NavigateToContent();
    }

    ed::BeginNode(ed::NodeId(1));
    ImGui::TextUnformatted("Test Node");

    ed::BeginPin(ed::PinId(2), ed::PinKind::Input);
    ImGui::Text("-> In");
    ed::EndPin();

    ed::BeginPin(ed::PinId(3), ed::PinKind::Output);
    ImGui::Text("Out ->");
    ed::EndPin();

    ed::EndNode();

    ed::End();
    ed::SetCurrentEditor(nullptr);
}
