#include "mainEditor.h"
#include "imgui.h"
#include <fstream>
#include <string>
#include <algorithm>

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

        m_nodes.push_back(CreateSimpleNode(gen, "Node A", ImVec2(60, 60)));
        m_nodes.push_back(CreateSimpleNode(gen, "Node B", ImVec2(320, 60)));

        ed::NavigateToContent();
    }

    for (auto& n : m_nodes)
        if (n.alive) DrawSimpleNode(n);
        
    for (const auto& l : m_links)
    ed::Link(l.id, l.startPinId, l.endPinId);

    static bool backspaceDelete = false;
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace))
        backspaceDelete = true;

    const bool deleting = ed::BeginDelete() || backspaceDelete;
    if (deleting)
    {
        // delete links
        ed::LinkId linkId;
        while (ed::QueryDeletedLink(&linkId))
        {
            if (ed::AcceptDeletedItem())
            {
                auto it = std::remove_if(m_links.begin(), m_links.end(),
                    [&](const Link& l){ return l.id == linkId; });

                if (it != m_links.end())
                {
                    m_links.erase(it, m_links.end());
                    saveGraph("graph.txt");
                }
            }
        }

        // delete nodes
        ed::NodeId nodeId;
        while (ed::QueryDeletedNode(&nodeId))
        {
            if (ed::AcceptDeletedItem())
            {
                auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
                    [&](const SimpleNode& n){ return n.alive && n.id == nodeId; });

                if (it != m_nodes.end())
                {
                    removeLinksForNode(*it);
                    it->alive = false;
                    saveGraph("graph.txt");
                }
            }
        }

        ed::EndDelete();
        backspaceDelete = false;
    }

    // create new link
    if (ed::BeginCreate())
    {
        ed::PinId startPinId, endPinId;
        if (ed::QueryNewLink(&startPinId, &endPinId))
        {
        auto isOutput = [&](ed::PinId p) {
            for (auto& n : m_nodes) if (n.alive && n.outPin == p) return true;
            return false;
        };
        auto isInput = [&](ed::PinId p) {
            for (auto& n : m_nodes) if (n.alive && n.inPin == p) return true;
            return false;
        };

            bool ok = (isOutput(startPinId) && isInput(endPinId)) ||
                      (isOutput(endPinId) && isInput(startPinId));

            if (ok)
            {
                // accept link creation only if it has valid pin types connected
                if (ed::AcceptNewItem())
                {
                    ed::PinId outPin = isOutput(startPinId) ? startPinId : endPinId;
                    ed::PinId inPin  = isInput(endPinId) ? endPinId : startPinId;

                    m_links.push_back({ gen.NewLink(), outPin, inPin });
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

void MainEditor::removeLinksForNode(const SimpleNode& n)
{
    auto it = std::remove_if(m_links.begin(), m_links.end(),
        [&](const Link& l)
        {
            return l.startPinId == n.outPin || l.endPinId == n.inPin
                || l.startPinId == n.inPin  || l.endPinId == n.outPin;
        });

    if (it != m_links.end())
        m_links.erase(it, m_links.end());
}


SimpleNode CreateSimpleNode(IdGen& gen, std::string title, ImVec2 pos)
{
    SimpleNode n;
    n.id = gen.NewNode();
    n.inPin  = gen.NewPin();
    n.outPin = gen.NewPin();
    n.title = std::move(title);
    n.initialPos = pos;
    return n;
}

void DrawSimpleNode(SimpleNode& n)
{
    if (!n.positioned) {
        ed::SetNodePosition(n.id, n.initialPos);
        n.positioned = true;
    }

    ed::BeginNode(n.id);

    ImGui::TextUnformatted(n.title.c_str());

    ed::BeginPin(n.inPin, ed::PinKind::Input);
    ImGui::Text("-> In");
    ed::EndPin();

    ed::BeginPin(n.outPin, ed::PinKind::Output);
    ImGui::Text("Out ->");
    ed::EndPin();

    ed::EndNode();
}