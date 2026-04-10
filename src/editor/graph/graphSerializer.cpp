#include "graphSerializer.h"
#include "graphState.h"
#include "core/graphModel/visualNode.h"
#include "core/graphModel/link.h"
#include "core/registry/nodeFactory.h"
#include "core/registry/nodeRegistry.h"
#include "imgui_node_editor.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace ed = ax::NodeEditor;

// We store each field as one text token in the save file.
// Special characters are escaped so loading gives back the exact same text.
static std::string EncodeFieldValue(const std::string& value)
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() + 8);

    for (unsigned char ch : value)
    {
        const bool safe =
            std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~';

        if (safe)
        {
            out.push_back(static_cast<char>(ch));
        }
        else
        {
            out.push_back('%');
            out.push_back(kHex[(ch >> 4) & 0x0F]);
            out.push_back(kHex[ch & 0x0F]);
        }
    }

    return out;
}

static int HexToNibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static std::string DecodeFieldValue(const std::string& encoded)
{
    if (encoded.find('%') == std::string::npos)
        return encoded;

    std::string out;
    out.reserve(encoded.size());

    for (size_t i = 0; i < encoded.size(); ++i)
    {
        if (encoded[i] == '%' && i + 2 < encoded.size())
        {
            int hi = HexToNibble(encoded[i + 1]);
            int lo = HexToNibble(encoded[i + 2]);
            if (hi >= 0 && lo >= 0)
            {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }

        out.push_back(encoded[i]);
    }

    return out;
}

static void NormalizeSequencePins(VisualNode& n, IdGen& idGen)
{
    if (n.nodeType != NodeType::Sequence)
        return;

    std::vector<Pin> flowOut;
    flowOut.reserve(n.outPins.size());

    for (const Pin& p : n.outPins)
    {
        if (!p.isInput && p.type == PinType::Flow)
            flowOut.push_back(p);
    }

    if (flowOut.empty())
    {
        flowOut.push_back(MakePin(
            static_cast<uint32_t>(idGen.NewPin().Get()),
            n.id,
            n.nodeType,
            "Then 0",
            PinType::Flow,
            false
        ));
    }

    for (size_t i = 0; i < flowOut.size(); ++i)
        flowOut[i].name = "Then " + std::to_string(i);

    n.outPins = std::move(flowOut);
}

void GraphSerializer::Save(const GraphState& state, const char* path)
{
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return;

    // Write nodes first, then links.
    // On load, we rebuild the graph from these lines.
    for (const auto& n : state.GetNodes())
    {
        if (!n.alive) continue;

        const NodeDescriptor* desc = NodeRegistry::Get().Find(n.nodeType);
        const std::string saveToken = desc ? desc->saveToken : "Unknown";

        std::vector<uint32_t> pinIds;
        for (const Pin& p : n.inPins)  pinIds.push_back(static_cast<uint32_t>(p.id.Get()));
        for (const Pin& p : n.outPins) pinIds.push_back(static_cast<uint32_t>(p.id.Get()));

        out << "node "
            << n.id.Get() << " "
            << saveToken << " "
            << pinIds.size();

        for (uint32_t pid : pinIds) out << " " << pid;

        out << " " << n.fields.size();
        for (const NodeField& f : n.fields)
        {
            const std::string encodedName = EncodeFieldValue(f.name);
            const std::string encoded = EncodeFieldValue(f.value);
            out << " " << encodedName << "=" << encoded;
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

static NodeField* FindField(std::vector<NodeField>& fields, const char* name)
{
    for (auto& f : fields)
        if (f.name == name) return &f;
    return nullptr;
}

static bool ParseBoolValue(const std::string& s)
{
    if (s == "true" || s == "True" || s == "1")
        return true;
    if (s == "false" || s == "False" || s == "0")
        return false;

    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end && *end == '\0')
        return v != 0.0;

    return false;
}

static bool TryParseDoubleValue(const std::string& s, double& out)
{
    if (s.empty())
        return false;

    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end && *end == '\0')
    {
        out = v;
        return true;
    }

    return false;
}

static std::string DefaultConstantValueForType(PinType t)
{
    switch (t)
    {
        case PinType::Boolean: return "false";
        case PinType::Number:  return "0.0";
        case PinType::String:  return "";
        case PinType::Array:
        case PinType::Struct:  return "[]";
        default:               return "";
    }
}

void GraphSerializer::ApplyConstantTypeFromFields(VisualNode& n, bool resetValueOnTypeChange)
{
    if (n.nodeType != NodeType::Constant) return;

    // Keep field order predictable in UI: Type first, Value second.
    size_t typeIdx = n.fields.size();
    size_t valueIdx = n.fields.size();
    for (size_t i = 0; i < n.fields.size(); ++i)
    {
        if (n.fields[i].name == "Type")  typeIdx = i;
        if (n.fields[i].name == "Value") valueIdx = i;
    }
    if (typeIdx < n.fields.size() && valueIdx < n.fields.size() && typeIdx > valueIdx)
        std::swap(n.fields[typeIdx], n.fields[valueIdx]);

    NodeField* typeF  = FindField(n.fields, "Type");
    NodeField* valueF = FindField(n.fields, "Value");
    if (!typeF || !valueF) return;

    const std::string& t = typeF->value;
    const PinType oldValueType = valueF->valueType;

    auto setTypeAndNormalize = [&](PinType targetType)
    {
        if (resetValueOnTypeChange && oldValueType != targetType)
        {
            valueF->value = DefaultConstantValueForType(targetType);
        }
        else
        {
            if (targetType == PinType::Boolean)
            {
                const bool b = ParseBoolValue(valueF->value);
                valueF->value = b ? "true" : "false";
            }
            else if (targetType == PinType::Number)
            {
                double v = 0.0;
                if (TryParseDoubleValue(valueF->value, v))
                    valueF->value = std::to_string(v);
                else
                    valueF->value = "0.0";
            }
            else if (targetType == PinType::Array)
            {
                if (valueF->value.empty())
                    valueF->value = "[]";
            }
        }

        valueF->valueType = targetType;
        if (!n.outPins.empty()) n.outPins[0].type = targetType;
    };

    if (t == "Boolean")
    {
        setTypeAndNormalize(PinType::Boolean);
    }
    else if (t == "Number")
    {
        setTypeAndNormalize(PinType::Number);
    }
    else if (t == "String")
    {
        setTypeAndNormalize(PinType::String);
    }
    else if (t == "Array")
    {
        setTypeAndNormalize(PinType::Array);
    }
    else
    {
        // If a saved type is invalid, fall back to String.
        typeF->value = "String";
        setTypeAndNormalize(PinType::String);
    }
}

void GraphSerializer::Load(GraphState& state, const char* path)
{
    std::ifstream in(path);
    if (!in.is_open()) return;

    // Loading replaces the current graph.
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
            in >> nid >> nodeTypeStr;

            in >> pinCount;

            NodeType nodeType = NodeRegistry::Get().FindByToken(nodeTypeStr);
            if (nodeType == NodeType::Unknown)
            {
                // If this app version does not know the node type, skip it and continue.
                float pxDummy, pyDummy;
                for (int i = 0; i < pinCount; ++i) { int skip; in >> skip; }
                int fieldCountSkip; in >> fieldCountSkip;
                for (int i = 0; i < fieldCountSkip; ++i) { std::string skipPair; in >> skipPair; }
                in >> pxDummy >> pyDummy;
                continue;
            }

            std::vector<int> pinIds;
            pinIds.reserve(pinCount);
            for (int i = 0; i < pinCount; ++i)
            {
                int pid; in >> pid;
                pinIds.push_back(pid);
                maxId = std::max(maxId, pid);
            }

            int fieldCount; 
            in >> fieldCount;

            std::vector<std::pair<std::string, std::string>> fieldValues;
            fieldValues.reserve(fieldCount);

            for (int i = 0; i < fieldCount; ++i)
            {
                std::string pair; 
                in >> pair;

                auto eq = pair.find('=');
                if (eq != std::string::npos)
                {
                    std::string name = DecodeFieldValue(pair.substr(0, eq));
                    std::string val  = DecodeFieldValue(pair.substr(eq + 1));
                    fieldValues.push_back({ name, val });
                }
            }

            float px, py; 
            in >> px >> py;

            maxId = std::max(maxId, nid);

            VisualNode n = CreateNodeFromTypeWithIds(nodeType, nid, pinIds, ImVec2(px, py));

            // Copy saved field values into matching fields.
            for (auto& [name, val] : fieldValues)
            {
                bool applied = false;
                for (NodeField& f : n.fields)
                {
                    if (f.name == name)
                    {
                        f.value = val;
                        applied = true;
                        break;
                    }
                }

                // Keep extra struct/function fields so later refresh logic can reconnect them.
                if (!applied && (n.nodeType == NodeType::StructCreate
                    || n.nodeType == NodeType::StructRead
                    || n.nodeType == NodeType::StructWrite
                    || n.nodeType == NodeType::Function))
                    n.fields.push_back(NodeField{ name, PinType::String, val });
            }

            ApplyConstantTypeFromFields(n);
            NormalizeSequencePins(n, state.GetIdGen());

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
    state.ClearDirty();
}