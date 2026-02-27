#include "graphSerializer.h"
#include "graphState.h"
#include "../core/graph/visualNode.h"
#include "../core/graph/link.h"
#include "../core/registry/nodeFactory.h"
#include "imgui_node_editor.h"
#include <fstream>
#include <algorithm>

namespace ed = ax::NodeEditor;

// Serialization helpers for pin types
static const char* PinTypeToString(PinType t);
static PinType PinTypeFromString(const std::string& s);

void GraphSerializer::Save(const GraphState& state, const char* path)
{
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return;

    for (const auto& n : state.GetNodes())
    {
        if (!n.alive) continue;

        std::vector<uint32_t> pinIds;
        for (const Pin& p : n.inPins)  pinIds.push_back(static_cast<uint32_t>(p.id.Get()));
        for (const Pin& p : n.outPins) pinIds.push_back(static_cast<uint32_t>(p.id.Get()));

        out << "node "
            << n.id.Get() << " "
            << NodeTypeToString(n.nodeType) << " "
            << pinIds.size();

        for (uint32_t pid : pinIds) out << " " << pid;

        out << " " << n.fields.size();
        for (const NodeField& f : n.fields)
        {
            std::string encoded = f.value;
            for (char& c : encoded) if (c == ' ') c = '\x01';
            out << " " << f.name << "=" << encoded;
        }

        ImVec2 pos = ed::GetNodePosition(n.id);
        out << " " << pos.x << " " << pos.y << "\n";
    }

    for (const auto& l : state.GetLinks())
    {
        if (!l.alive) continue;
        out << "link "
            << l.id.Get() << " "
            << l.startPinId.Get() << " "
            << l.endPinId.Get() << " "
            << PinTypeToString(l.type) << "\n";
    }
}

void GraphSerializer::Load(GraphState& state, const char* path)
{
    std::ifstream in(path);
    if (!in.is_open()) return;

    state.Clear();

    int maxId = 0;
    std::string token;

    while (in >> token)
    {
        if (token == "node")
        {
            int         nid;
            std::string nodeTypeStr;
            int         pinCount;
            in >> nid >> nodeTypeStr >> pinCount;

            NodeType nodeType = NodeTypeFromString(nodeTypeStr);

            std::vector<int> pinIds;
            pinIds.reserve(pinCount);
            for (int i = 0; i < pinCount; ++i)
            {
                int pid; in >> pid;
                pinIds.push_back(pid);
                maxId = std::max(maxId, pid);
            }

            int fieldCount; in >> fieldCount;
            std::vector<std::pair<std::string, std::string>> fieldValues;
            for (int i = 0; i < fieldCount; ++i)
            {
                std::string pair; in >> pair;
                auto eq = pair.find('=');
                if (eq != std::string::npos)
                {
                    std::string name = pair.substr(0, eq);
                    std::string val  = pair.substr(eq + 1);
                    for (char& c : val) if (c == '\x01') c = ' ';
                    fieldValues.push_back({ name, val });
                }
            }

            float px, py; in >> px >> py;
            maxId = std::max(maxId, nid);

            VisualNode n = CreateNodeFromTypeWithIds(nodeType, nid, pinIds, ImVec2(px, py));

            for (auto& [name, val] : fieldValues)
                for (NodeField& f : n.fields)
                    if (f.name == name) { f.value = val; break; }

            state.AddNode(n);
        }
        else if (token == "link")
        {
            int         id, start, end;
            std::string pinTypeStr;
            in >> id >> start >> end >> pinTypeStr;

            Link lnk;
            lnk.id         = ed::LinkId(id);
            lnk.startPinId = ed::PinId(start);
            lnk.endPinId   = ed::PinId(end);
            lnk.type       = PinTypeFromString(pinTypeStr);
            state.AddLink(lnk);

            maxId = std::max({ maxId, id, start, end });
        }
        else
        {
            std::string dummy;
            std::getline(in, dummy);
        }
    }

    state.GetIdGen().SetNext(maxId + 1);
}

// ---- Serialization helpers ----

static const char* PinTypeToString(PinType t)
{
    switch (t)
    {
        case PinType::Number:   return "Number";
        case PinType::Boolean:  return "Boolean";
        case PinType::String:   return "String";
        case PinType::Array:    return "Array";
        case PinType::Function: return "Function";
        case PinType::Flow:     return "Flow";
        default:                return "Any";
    }
}

static PinType PinTypeFromString(const std::string& s)
{
    if (s == "Number")   return PinType::Number;
    if (s == "Boolean")  return PinType::Boolean;
    if (s == "String")   return PinType::String;
    if (s == "Array")    return PinType::Array;
    if (s == "Function") return PinType::Function;
    if (s == "Flow")     return PinType::Flow;
    return PinType::Any;
}
