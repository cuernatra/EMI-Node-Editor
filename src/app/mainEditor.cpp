#include "mainEditor.h"
#include "imgui.h"
#include <fstream>
#include <string>

MainEditor::MainEditor()
{
    ed::Config config;
    config.SettingsFile = "node_editor.json";
    m_ctx = ed::CreateEditor(&config);

    loadGraph("graph.txt");
}

MainEditor::~MainEditor()
{
    saveGraph("graph.txt");
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
        ed::SetNodePosition(ed::NodeId(10), ImVec2(320, 60));
        ed::NavigateToContent();
    }

    // Node 1
    ed::BeginNode(ed::NodeId(1));
    ImGui::TextUnformatted("Node A");

    ed::BeginPin(ed::PinId(2), ed::PinKind::Input);
    ImGui::Text("-> In");
    ed::EndPin();

    ed::BeginPin(ed::PinId(3), ed::PinKind::Output);
    ImGui::Text("Out ->");
    ed::EndPin();

    ed::EndNode();

    // Node 2
    ed::BeginNode(ed::NodeId(10));
    ImGui::TextUnformatted("Node B");

    ed::BeginPin(ed::PinId(5), ed::PinKind::Input);
    ImGui::Text("-> In");
    ed::EndPin();

    ed::BeginPin(ed::PinId(6), ed::PinKind::Output);
    ImGui::Text("Out ->");
    ed::EndPin();

    ed::EndNode();

    for (const auto& l : m_links)
        ed::Link(l.id, l.startPinId, l.endPinId);

    // create new link
    if (ed::BeginCreate())
    {
        ed::PinId startPinId, endPinId;
        if (ed::QueryNewLink(&startPinId, &endPinId))
        {
            auto isOutput = [](ed::PinId p) { return p.Get() == 3 || p.Get() == 6; };
            auto isInput  = [](ed::PinId p) { return p.Get() == 2 || p.Get() == 5; };

            bool ok = (isOutput(startPinId) && isInput(endPinId)) ||
                      (isOutput(endPinId) && isInput(startPinId));

            if (ok)
            {
                // accept link creation only if it has valid pin types connected
                if (ed::AcceptNewItem())
                {
                    ed::PinId outPin = isOutput(startPinId) ? startPinId : endPinId;
                    ed::PinId inPin  = isInput(endPinId) ? endPinId : startPinId;

                    m_links.push_back({ ed::LinkId(m_nextLinkId++), outPin, inPin });
                    saveGraph("graph.txt");
                }
            }
            else
            {
                ed::RejectNewItem();
            }
        }
    }
    ed::EndCreate();

    ed::End();
    ed::SetCurrentEditor(nullptr);
}

void MainEditor::saveGraph(const char* path) const
{
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open())
        return;

    out << "nextLinkId " << m_nextLinkId << "\n";
    for (const auto& l : m_links)
    {
        out << "link " << l.id.Get()
            << " " << l.startPinId.Get()
            << " " << l.endPinId.Get() << "\n";
    }
}

void MainEditor::loadGraph(const char* path)
{
    std::ifstream in(path);
    if (!in.is_open())
        return;

    m_links.clear();

    std::string type;
    while (in >> type)
    {
        if (type == "nextLinkId")
        {
            in >> m_nextLinkId;
        }
        else if (type == "link")
        {
            int id, start, end;
            in >> id >> start >> end;
            m_links.push_back({ ed::LinkId(id), ed::PinId(start), ed::PinId(end) });
        }
        else
        {
            std::string dummy;
            std::getline(in, dummy);
        }
    }
}