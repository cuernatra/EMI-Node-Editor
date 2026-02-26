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

    ed::Suspend();

    const ImGuiPayload* pl = ImGui::GetDragDropPayload();
    const bool dragging_spawn_node = pl && pl->IsDataType("SPAWN_TEST_NODE");

    if(dragging_spawn_node)
    {
        ImVec2 dropSize = ImGui::GetContentRegionAvail();
        if (dropSize.x < 1) dropSize.x = 1;
        if (dropSize.y < 1) dropSize.y = 1;

        ImGui::InvisibleButton("##editor_drop_area", dropSize);

        if(ImGui::BeginDragDropTarget())
        {
            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SPAWN_TEST_NODE"))
            {
                const char* title = (const char*)payload->Data;

                ImVec2 screenPos = ImGui::GetMousePos();
                ImVec2 canvasPos = ed::ScreenToCanvas(screenPos);

                m_nodes.push_back(CreateVisualNode(gen, title, canvasPos, "test_type", "test_data"));
            }
            ImGui::EndDragDropTarget();
        }
    }

    ed::Resume();

    for(auto& n : m_nodes)
    {
        if(n.alive) DrawVisualNode(n);
    }
        
    for(const auto& l : m_links)
    {
        ed::Link(l.id, l.startPinId, l.endPinId);
    }
        
    static bool backspaceDelete = false;

    if(ImGui::IsKeyPressed(ImGuiKey_Backspace))
    {
        backspaceDelete = true;
    }
        
    const bool deleting = ed::BeginDelete() || backspaceDelete;

    if(deleting)
    {
        // delete links
        ed::LinkId linkId;
        deleteLinks(linkId);

        // delete nodes
        ed::NodeId nodeId;
        deleteNodes(nodeId);
        ed::EndDelete();
        backspaceDelete = false;
    }

    // create new link
    if(ed::BeginCreate())
    {
        createNewLink();
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

    for (const auto& n : m_nodes)
    {
        if (!n.alive) continue;

        out << "node "
            << n.id.Get() << " "
            << n.inPin.Get() << " "
            << n.outPin.Get() << " "
            << n.title << "\n";
    }

    for (const auto& l : m_links)
    {
        out << "link "
            << l.id.Get() << " "
            << l.startPinId.Get() << " "
            << l.endPinId.Get() << "\n";
    }
}


void MainEditor::loadGraph(const char* path)
{
    std::ifstream in(path);
    if (!in.is_open())
        return;

    m_nodes.clear();
    m_links.clear();

    int maxId = 0;

    std::string type;
    while (in >> type)
    {
        if (type == "nextLinkId")
        {
            in >> m_nextLinkId;
        }
        else if (type == "node")
        {
            int nid, inPin, outPin;
            std::string title;

            in >> nid >> inPin >> outPin;
            in >> std::ws;
            std::getline(in, title);

            m_nodes.push_back(
                CreateVisualNodeWithId(nid, inPin, outPin, title, ImVec2(0,0), "test_type", "test_data")
            );

            // all possible id places
            maxId = std::max(maxId, nid);
            maxId = std::max(maxId, inPin);
            maxId = std::max(maxId, outPin);
        }
        else if (type == "link")
        {
            int id, start, end;
            in >> id >> start >> end;

            m_links.push_back({
                ed::LinkId(id),
                ed::PinId(start),
                ed::PinId(end)
            });

            maxId = std::max(maxId, id);
            maxId = std::max(maxId, start);
            maxId = std::max(maxId, end);
        }
        else
        {
            std::string dummy;
            std::getline(in, dummy);
        }
    }

    gen.SetNext(maxId + 1);
}

void MainEditor::removeLinksForNode(const VisualNode& n)
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

void MainEditor::createNewLink()
{
    ed::PinId startPinId, endPinId;
    if(ed::QueryNewLink(&startPinId, &endPinId))
    {
        auto isOutput = [&](ed::PinId p) 
        {
            for (auto& n : m_nodes)
            {
                if (n.alive && n.outPin == p) return true;
            } 
            return false;
        };

        auto isInput = [&](ed::PinId p) 
        {
            for(auto& n : m_nodes) 
            {
                if (n.alive && n.inPin == p) return true;
            }
            return false;
        };

        bool ok = (isOutput(startPinId) && 
            isInput(endPinId)) || (isOutput(endPinId) && isInput(startPinId));

        if(ok)
        {
            // accept link creation only if it has valid pin types connected
            if(ed::AcceptNewItem())
            {
                ed::PinId outPin = isOutput(startPinId) ? startPinId : endPinId;
                ed::PinId inPin  = isInput(endPinId) ? endPinId : startPinId;

                m_links.push_back({gen.NewLink(), outPin, inPin});
                saveGraph("graph.txt");
            }
        }
        else
        {
            ed::RejectNewItem();
        }
    }
}

void MainEditor::deleteNodes(ed::NodeId nodeId)
{
    while(ed::QueryDeletedNode(&nodeId))
    {
        if (ed::AcceptDeletedItem())
        {
            auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
                [&](const VisualNode& n){ return n.alive && n.id == nodeId; });

            if(it != m_nodes.end())
            {
                removeLinksForNode(*it);
                it->alive = false;
                saveGraph("graph.txt");
            }
        }
    }
}

void MainEditor::deleteLinks(ed::LinkId linkId)
{
    while(ed::QueryDeletedLink(&linkId))
    {
        if(ed::AcceptDeletedItem())
        {
            auto it = std::remove_if(m_links.begin(), m_links.end(),
                [&](const Link& l){ return l.id == linkId; });

            if(it != m_links.end())
            {
                m_links.erase(it, m_links.end());
                saveGraph("graph.txt");
            }
        }
    }
}