#include "graphSerializer.h"
#include "graphState.h"
#include "../core/graph/visualNode.h"
#include "../core/graph/link.h"
#include "../core/registry/nodeFactory.h"
#include "imgui_node_editor.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace ed = ax::NodeEditor;

// Serialization helpers for pin types
static const char* PinTypeToString(PinType t);
static PinType PinTypeFromString(const std::string& s);

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
    // Backward compatibility: accept older "esc:" prefix but do not require it.
    const std::string& body = (encoded.rfind("esc:", 0) == 0) ? encoded.substr(4) : encoded;

    if (body.find('%') == std::string::npos)
        return body;

    std::string out;
    out.reserve(body.size());

    for (size_t i = 0; i < body.size(); ++i)
    {
        if (body[i] == '%' && i + 2 < body.size())
        {
            int hi = HexToNibble(body[i + 1]);
            int lo = HexToNibble(body[i + 2]);
            if (hi >= 0 && lo >= 0)
            {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }

        out.push_back(body[i]);
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

static void NormalizeFunctionCallPins(VisualNode& n, IdGen& idGen)
{
    if (n.nodeType != NodeType::FunctionCall)
        return;

    // Read ArgCount from fields.
    int argCount = 0;
    for (const NodeField& f : n.fields)
    {
        if (f.name == "ArgCount")
        {
            try { argCount = static_cast<int>(std::stod(f.value)); } catch (...) {}
            break;
        }
    }
    if (argCount < 0) argCount = 0;

    // Rebuild inPins: keep In (Flow), then Arg0..Arg{argCount-1}.
    Pin flowIn;
    bool hasFlowIn = false;
    for (const Pin& p : n.inPins)
    {
        if (p.type == PinType::Flow && p.name == "In") { flowIn = p; hasFlowIn = true; }
    }
    if (!hasFlowIn)
    {
        flowIn = MakePin(
            static_cast<uint32_t>(idGen.NewPin().Get()),
            n.id, n.nodeType, "In", PinType::Flow, true);
    }

    std::vector<Pin> argPins;
    int existingArgCount = 0;
    for (const Pin& p : n.inPins)
        if (p.name.rfind("Arg", 0) == 0) { argPins.push_back(p); ++existingArgCount; }

    // Add missing Arg pins.
    while (static_cast<int>(argPins.size()) < argCount)
    {
        const int idx = static_cast<int>(argPins.size());
        argPins.push_back(MakePin(
            static_cast<uint32_t>(idGen.NewPin().Get()),
            n.id, n.nodeType,
            "Arg" + std::to_string(idx),
            PinType::Any, true));
    }
    // Remove excess Arg pins.
    if (static_cast<int>(argPins.size()) > argCount)
        argPins.resize(static_cast<size_t>(argCount));

    n.inPins.clear();
    n.inPins.push_back(flowIn);
    for (const Pin& p : argPins)
        n.inPins.push_back(p);

    // Rebuild outPins: keep Out (Flow) and Result (Any).
    Pin flowOut, resultPin;
    bool hasOut = false, hasResult = false;
    for (const Pin& p : n.outPins)
    {
        if (!hasOut    && p.type == PinType::Flow && p.name == "Out")   { flowOut   = p; hasOut    = true; }
        if (!hasResult && p.type == PinType::Any  && p.name == "Result"){ resultPin = p; hasResult = true; }
    }
    if (!hasOut)
        flowOut = MakePin(static_cast<uint32_t>(idGen.NewPin().Get()), n.id, n.nodeType, "Out",    PinType::Flow, false);
    if (!hasResult)
        resultPin = MakePin(static_cast<uint32_t>(idGen.NewPin().Get()), n.id, n.nodeType, "Result", PinType::Any,  false);

    n.outPins.clear();
    n.outPins.push_back(flowOut);
    n.outPins.push_back(resultPin);
}

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
            const std::string encoded = EncodeFieldValue(f.value);
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
        case PinType::Array:   return "[]";
        default:               return "";
    }
}

void GraphSerializer::ApplyConstantTypeFromFields(VisualNode& n, bool resetValueOnTypeChange)
{
    if (n.nodeType != NodeType::Constant) return;

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
            // Keep backward compatibility with older/invalid values when not explicitly resetting.
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
            // String keeps current text as-is.
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
        // Keep Constant type set constrained to sane data values.
        // Legacy/invalid values (Flow/Function/Any/unknown) are normalized to String.
        typeF->value = "String";
        setTypeAndNormalize(PinType::String);
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
                    std::string name = pair.substr(0, eq);
                    std::string val  = DecodeFieldValue(pair.substr(eq + 1));
                    fieldValues.push_back({ name, val });
                }
            }

            float px, py; 
            in >> px >> py;

            maxId = std::max(maxId, nid);

            VisualNode n = CreateNodeFromTypeWithIds(nodeType, nid, pinIds, ImVec2(px, py));

            // Apply loaded field values
            for (auto& [name, val] : fieldValues)
            {
                for (NodeField& f : n.fields)
                {
                    if (f.name == name)
                    {
                        f.value = val;
                        break;
                    }
                }
            }

            ApplyConstantTypeFromFields(n);
            NormalizeSequencePins(n, state.GetIdGen());
            NormalizeFunctionCallPins(n, state.GetIdGen());

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
