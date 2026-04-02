#include "graphEditorUtils.h"
#include "core/graph/link.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <unordered_map>

namespace
{
std::string TrimCopy(const std::string& s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::vector<std::string> ParseArrayItems(const std::string& text)
{
    std::string src = TrimCopy(text);
    if (src.size() >= 2 && src.front() == '[' && src.back() == ']')
        src = src.substr(1, src.size() - 2);

    std::vector<std::string> out;
    std::string current;
    current.reserve(src.size());

    int bracketDepth = 0;
    int parenDepth = 0;
    int braceDepth = 0;
    bool inQuote = false;
    char quoteChar = '\0';
    bool escape = false;

    auto pushCurrent = [&]() {
        std::string item = TrimCopy(current);
        if (!item.empty())
            out.push_back(item);
        current.clear();
    };

    for (char ch : src)
    {
        if (escape)
        {
            current.push_back(ch);
            escape = false;
            continue;
        }

        if (inQuote)
        {
            current.push_back(ch);
            if (ch == '\\')
                escape = true;
            else if (ch == quoteChar)
                inQuote = false;
            continue;
        }

        if (ch == '"' || ch == '\'')
        {
            inQuote = true;
            quoteChar = ch;
            current.push_back(ch);
            continue;
        }

        if (ch == '[') ++bracketDepth;
        else if (ch == ']') bracketDepth = std::max(0, bracketDepth - 1);
        else if (ch == '(') ++parenDepth;
        else if (ch == ')') parenDepth = std::max(0, parenDepth - 1);
        else if (ch == '{') ++braceDepth;
        else if (ch == '}') braceDepth = std::max(0, braceDepth - 1);

        if (ch == ',' && bracketDepth == 0 && parenDepth == 0 && braceDepth == 0)
        {
            pushCurrent();
            continue;
        }

        current.push_back(ch);
    }

    pushCurrent();
    return out;
}

std::string BuildArrayString(const std::vector<std::string>& items)
{
    std::string out = "[";
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (i != 0)
            out += ", ";
        out += TrimCopy(items[i]);
    }
    out += "]";
    return out;
}

PinType VariableTypeFromString(const std::string& typeName);
const char* VariableTypeToString(PinType type);

struct StructFieldDef
{
    std::string name;
    PinType type = PinType::Any;
};

std::vector<StructFieldDef> ParseStructFieldDefs(const std::string& text)
{
    std::vector<StructFieldDef> defs;
    const std::vector<std::string> items = ParseArrayItems(text);
    for (const std::string& raw : items)
    {
        std::string item = TrimCopy(raw);
        if (item.size() >= 2 && ((item.front() == '"' && item.back() == '"') || (item.front() == '\'' && item.back() == '\'')))
            item = item.substr(1, item.size() - 2);

        const size_t sep = item.find(':');
        if (sep == std::string::npos)
            continue;

        StructFieldDef def;
        def.name = TrimCopy(item.substr(0, sep));
        const std::string typeName = TrimCopy(item.substr(sep + 1));
        def.type = VariableTypeFromString(typeName);
        if (!def.name.empty())
            defs.push_back(def);
    }
    return defs;
}

std::string BuildStructFieldDefsString(const std::vector<StructFieldDef>& defs)
{
    std::vector<std::string> items;
    items.reserve(defs.size());
    for (const StructFieldDef& def : defs)
        items.push_back(std::string("\"") + def.name + ":" + VariableTypeToString(def.type) + "\"");
    return BuildArrayString(items);
}

NodeField* EnsureField(std::vector<NodeField>& fields, const std::string& name, PinType type, const std::string& defaultValue, bool& changed)
{
    if (NodeField* f = GraphEditorUtils::FindField(fields, name.c_str()))
    {
        if (f->valueType != type)
        {
            f->valueType = type;
            changed = true;
        }
        return f;
    }

    fields.push_back(NodeField{ name, type, defaultValue });
    changed = true;
    return &fields.back();
}

bool TryParseDouble(const std::string& s, double& out);
bool ParseBoolLoose(const std::string& s);

PinType VariableTypeFromString(const std::string& typeName)
{
    if (typeName == "Any")     return PinType::Any;
    if (typeName == "Boolean") return PinType::Boolean;
    if (typeName == "String")  return PinType::String;
    if (typeName == "Array")   return PinType::Array;
    return PinType::Number;
}

bool TryParseDouble(const std::string& s, double& out)
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

bool ParseBoolLoose(const std::string& s)
{
    if (s == "true" || s == "True" || s == "1")
        return true;
    if (s == "false" || s == "False" || s == "0")
        return false;

    double v = 0.0;
    if (TryParseDouble(s, v))
        return v != 0.0;

    return false;
}

std::string DefaultValueForPinType(PinType t)
{
    switch (t)
    {
        case PinType::Boolean: return "false";
        case PinType::String:  return "";
        case PinType::Array:   return "[]";
        case PinType::Number:
        default:               return "0.0";
    }
}

void NormalizeValueForPinType(PinType t, std::string& value)
{
    switch (t)
    {
        case PinType::Boolean:
            value = ParseBoolLoose(value) ? "true" : "false";
            break;

        case PinType::Number:
        {
            double v = 0.0;
            if (TryParseDouble(value, v))
                value = std::to_string(v);
            else
                value = "0.0";
            break;
        }

        case PinType::Array:
            if (value.empty())
                value = "[]";
            break;

        case PinType::String:
        default:
            break;
    }
}

const char* VariableTypeToString(PinType type)
{
    switch (type)
    {
        case PinType::Any:     return "Any";
        case PinType::Boolean: return "Boolean";
        case PinType::String:  return "String";
        case PinType::Array:   return "Array";
        case PinType::Number:
        default:               return "Number";
    }
}

void RemovePinsByNameExcept(std::vector<Pin>& pins, const std::vector<std::string>& keepNames)
{
    pins.erase(
        std::remove_if(pins.begin(), pins.end(), [&](const Pin& p)
        {
            for (const std::string& keep : keepNames)
                if (p.name == keep)
                    return false;
            return true;
        }),
        pins.end()
    );
}

const Pin* FindIncomingPinSource(const GraphState& state, const std::vector<Link>& links, const Pin& inputPin)
{
    for (const Link& l : links)
    {
        if (!l.alive || l.endPinId != inputPin.id)
            continue;
        return state.FindPin(l.startPinId);
    }
    return nullptr;
}
}

namespace GraphEditorUtils
{
ImVec2 SnapToNodeGrid(const ImVec2& pos)
{
    constexpr float kGridStep = 32.0f;
    return ImVec2(
        std::round(pos.x / kGridStep) * kGridStep,
        std::round(pos.y / kGridStep) * kGridStep
    );
}

void ParseSpawnPayloadTitle(const char* payloadTitle, NodeType& outType, std::string& outVariableVariant)
{
    outType = NodeTypeFromString(payloadTitle ? payloadTitle : "");
    outVariableVariant.clear();

    if (!payloadTitle)
        return;

    const std::string raw(payloadTitle);
    const size_t sep = raw.find(':');
    if (sep == std::string::npos)
        return;

    const std::string base = raw.substr(0, sep);
    const std::string suffix = raw.substr(sep + 1);

    const NodeType parsedBase = NodeTypeFromString(base);
    if (parsedBase == NodeType::Unknown)
        return;

    outType = parsedBase;
    if (parsedBase == NodeType::Variable)
        outVariableVariant = suffix;
}

NodeField* FindField(std::vector<NodeField>& fields, const char* name)
{
    for (NodeField& f : fields)
        if (f.name == name)
            return &f;
    return nullptr;
}

const NodeField* FindField(const std::vector<NodeField>& fields, const char* name)
{
    for (const NodeField& f : fields)
        if (f.name == name)
            return &f;
    return nullptr;
}

Pin* FindPinByName(std::vector<Pin>& pins, const char* name)
{
    for (Pin& p : pins)
        if (p.name == name)
            return &p;
    return nullptr;
}

bool RefreshVariableNodeTypes(GraphState& state)
{
    bool changed = false;

    auto& nodes = state.GetNodes();
    const auto& links = state.GetLinks();

    struct VariableDef
    {
        PinType type = PinType::Number;
        std::string typeName = "Number";
    };

    std::unordered_map<std::string, VariableDef> definitions;

    for (auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Variable)
            continue;

        NodeField* variantField = FindField(n.fields, "Variant");
        NodeField* nameField = FindField(n.fields, "Name");
        NodeField* typeField = FindField(n.fields, "Type");
        NodeField* defaultField = FindField(n.fields, "Default");
        const std::string variant = variantField ? variantField->value : "Set";
        if (variant != "Set")
            continue;

        if (variantField && variantField->value != "Set")
        {
            variantField->value = "Set";
            changed = true;
        }

        if (n.title != "Set Variable")
        {
            n.title = "Set Variable";
            changed = true;
        }

        Pin* flowInPin = FindPinByName(n.inPins, "In");
        if (!flowInPin)
        {
            n.inPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "In",
                PinType::Flow,
                true
            ));
            flowInPin = &n.inPins.back();
            changed = true;
        }
        else if (flowInPin->type != PinType::Flow)
        {
            flowInPin->type = PinType::Flow;
            changed = true;
        }

        Pin* setPin = FindPinByName(n.inPins, "Default");
        if (!setPin)
            setPin = FindPinByName(n.inPins, "Set");
        if (!setPin)
        {
            n.inPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "Default",
                PinType::Any,
                true
            ));
            setPin = &n.inPins.back();
            changed = true;
        }
        else if (setPin->name != "Default")
        {
            setPin->name = "Default";
            changed = true;
        }

        Pin* flowOutPin = FindPinByName(n.outPins, "Out");
        if (!flowOutPin)
        {
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "Out",
                PinType::Flow,
                false
            ));
            flowOutPin = &n.outPins.back();
            changed = true;
        }
        else if (flowOutPin->type != PinType::Flow)
        {
            flowOutPin->type = PinType::Flow;
            changed = true;
        }

        Pin* valuePin = FindPinByName(n.outPins, "Value");
        if (!valuePin)
        {
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "Value",
                PinType::Any,
                false
            ));
            valuePin = &n.outPins.back();
            changed = true;
        }

        PinType resolvedType = VariableTypeFromString(typeField ? typeField->value : "Number");
        if (setPin)
        {
            if (const Pin* source = FindIncomingPinSource(state, links, *setPin))
            {
                if (source->type != PinType::Any && source->type != PinType::Flow)
                    resolvedType = source->type;
            }
        }

        const char* resolvedTypeName = VariableTypeToString(resolvedType);
        if (typeField && typeField->value != resolvedTypeName)
        {
            typeField->value = resolvedTypeName;
            changed = true;
        }

        if (defaultField)
        {
            const bool typeChanged = (defaultField->valueType != resolvedType);
            defaultField->valueType = resolvedType;

            if (typeChanged)
            {
                defaultField->value = DefaultValueForPinType(resolvedType);
                changed = true;
            }
            else
            {
                const std::string before = defaultField->value;
                NormalizeValueForPinType(resolvedType, defaultField->value);
                if (defaultField->value != before)
                    changed = true;
            }
        }

        if (setPin && setPin->type != resolvedType)
        {
            setPin->type = resolvedType;
            changed = true;
        }
        if (valuePin && valuePin->type != resolvedType)
        {
            valuePin->type = resolvedType;
            changed = true;
        }

        const std::string varName = nameField ? nameField->value : "myVar";
        definitions[varName] = VariableDef{
            resolvedType,
            resolvedTypeName
        };
    }

    for (auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Variable)
            continue;

        NodeField* variantField = FindField(n.fields, "Variant");
        NodeField* nameField = FindField(n.fields, "Name");
        NodeField* typeField = FindField(n.fields, "Type");
        const std::string variant = variantField ? variantField->value : "Set";
        const std::string varName = nameField ? nameField->value : "";

        if (variant == "Get")
        {
            if (variantField && variantField->value != "Get")
            {
                variantField->value = "Get";
                changed = true;
            }

            if (n.title != "Get Variable")
            {
                n.title = "Get Variable";
                changed = true;
            }

            if (nameField && definitions.empty())
            {
                if (!nameField->value.empty())
                {
                    nameField->value.clear();
                    changed = true;
                }
            }
            else if (nameField && !definitions.empty() && definitions.find(nameField->value) == definitions.end())
            {
                nameField->value = definitions.begin()->first;
                changed = true;
            }

            const std::string resolvedName = nameField ? nameField->value : "";
            const auto it = definitions.find(resolvedName);
            const PinType getType = (it != definitions.end())
                ? it->second.type
                : VariableTypeFromString(typeField ? typeField->value : "Number");

            const char* getTypeName = VariableTypeToString(getType);
            if (typeField && typeField->value != getTypeName)
            {
                typeField->value = getTypeName;
                changed = true;
            }

            if (!n.inPins.empty())
            {
                n.inPins.clear();
                changed = true;
            }

            const size_t outBefore = n.outPins.size();
            RemovePinsByNameExcept(n.outPins, { "Value" });
            if (n.outPins.size() != outBefore)
                changed = true;

            Pin* valuePin = FindPinByName(n.outPins, "Value");
            if (!valuePin)
            {
                n.outPins.push_back(MakePin(
                    static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                    n.id,
                    n.nodeType,
                    "Value",
                    getType,
                    false
                ));
                valuePin = &n.outPins.back();
                changed = true;
            }
            if (valuePin && valuePin->type != getType)
            {
                valuePin->type = getType;
                changed = true;
            }
        }
        else
        {
            if (variantField && variantField->value != "Set")
            {
                variantField->value = "Set";
                changed = true;
            }

            if (n.title != "Set Variable")
            {
                n.title = "Set Variable";
                changed = true;
            }

            const size_t inBefore = n.inPins.size();
            RemovePinsByNameExcept(n.inPins, { "In", "Default" });
            if (n.inPins.size() != inBefore)
                changed = true;

            const size_t outBefore = n.outPins.size();
            RemovePinsByNameExcept(n.outPins, { "Out", "Value" });
            if (n.outPins.size() != outBefore)
                changed = true;
        }
    }

    return changed;
}

bool RefreshOutputNodeInputTypes(GraphState& state)
{
    bool changed = false;

    auto& nodes = state.GetNodes();
    const auto& links = state.GetLinks();

    for (auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Output)
            continue;

        Pin* valuePin = FindPinByName(n.inPins, "Value");
        if (!valuePin)
            continue;

        PinType resolvedType = PinType::Any;
        for (const Link& l : links)
        {
            if (!l.alive || l.endPinId != valuePin->id)
                continue;

            const Pin* src = state.FindPin(l.startPinId);
            if (src && src->type != PinType::Flow)
            {
                resolvedType = src->type;
                break;
            }
        }

        if (valuePin->type != resolvedType)
        {
            valuePin->type = resolvedType;
            changed = true;
        }
    }

    return changed;
}

bool RefreshLoopNodeLayout(GraphState& state)
{
    bool changed = false;

    auto& nodes = state.GetNodes();

    for (auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Loop)
            continue;

        if (n.title != "Loop")
        {
            n.title = "Loop";
            changed = true;
        }

        const size_t inBefore = n.inPins.size();
        RemovePinsByNameExcept(n.inPins, { "In", "Start", "Count" });
        if (n.inPins.size() != inBefore)
            changed = true;

        const size_t outBefore = n.outPins.size();
        RemovePinsByNameExcept(n.outPins, { "Body", "Completed", "Index" });
        if (n.outPins.size() != outBefore)
            changed = true;

        Pin* inPin = FindPinByName(n.inPins, "In");
        if (!inPin)
        {
            n.inPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "In",
                PinType::Flow,
                true
            ));
            inPin = &n.inPins.back();
            changed = true;
        }
        else if (inPin->type != PinType::Flow)
        {
            inPin->type = PinType::Flow;
            changed = true;
        }

        Pin* startPin = FindPinByName(n.inPins, "Start");
        if (!startPin)
        {
            n.inPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "Start",
                PinType::Number,
                true
            ));
            startPin = &n.inPins.back();
            changed = true;
        }
        else if (startPin->type != PinType::Number)
        {
            startPin->type = PinType::Number;
            changed = true;
        }

        Pin* countPin = FindPinByName(n.inPins, "Count");
        if (!countPin)
        {
            n.inPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "Count",
                PinType::Number,
                true
            ));
            countPin = &n.inPins.back();
            changed = true;
        }
        else if (countPin->type != PinType::Number)
        {
            countPin->type = PinType::Number;
            changed = true;
        }

        Pin* bodyPin = FindPinByName(n.outPins, "Body");
        if (!bodyPin)
        {
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "Body",
                PinType::Flow,
                false
            ));
            bodyPin = &n.outPins.back();
            changed = true;
        }
        else if (bodyPin->type != PinType::Flow)
        {
            bodyPin->type = PinType::Flow;
            changed = true;
        }

        Pin* completedPin = FindPinByName(n.outPins, "Completed");
        if (!completedPin)
        {
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "Completed",
                PinType::Flow,
                false
            ));
            completedPin = &n.outPins.back();
            changed = true;
        }
        else if (completedPin->type != PinType::Flow)
        {
            completedPin->type = PinType::Flow;
            changed = true;
        }

        Pin* indexPin = FindPinByName(n.outPins, "Index");
        if (!indexPin)
        {
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "Index",
                PinType::Number,
                false
            ));
            indexPin = &n.outPins.back();
            changed = true;
        }
        else if (indexPin->type != PinType::Number)
        {
            indexPin->type = PinType::Number;
            changed = true;
        }

        NodeField* startField = FindField(n.fields, "Start");
        if (!startField)
        {
            n.fields.push_back(NodeField{ "Start", PinType::Number, "0" });
            startField = &n.fields.back();
            changed = true;
        }
        else
        {
            const bool typeChanged = (startField->valueType != PinType::Number);
            startField->valueType = PinType::Number;
            if (typeChanged)
                changed = true;

            const std::string before = startField->value;
            NormalizeValueForPinType(PinType::Number, startField->value);
            if (startField->value != before)
                changed = true;
        }

        NodeField* countField = FindField(n.fields, "Count");
        if (!countField)
        {
            n.fields.push_back(NodeField{ "Count", PinType::Number, "0" });
            countField = &n.fields.back();
            changed = true;
        }
        else
        {
            const bool typeChanged = (countField->valueType != PinType::Number);
            countField->valueType = PinType::Number;
            if (typeChanged)
                changed = true;

            const std::string before = countField->value;
            NormalizeValueForPinType(PinType::Number, countField->value);
            if (countField->value != before)
                changed = true;
        }
    }

    return changed;
}

bool RefreshForEachNodeLayout(GraphState& state)
{
    bool changed = false;

    auto& nodes = state.GetNodes();

    for (auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::ForEach)
            continue;

        if (n.title != "For Each")
        {
            n.title = "For Each";
            changed = true;
        }

        const size_t inBefore = n.inPins.size();
        RemovePinsByNameExcept(n.inPins, { "In", "Array" });
        if (n.inPins.size() != inBefore)
            changed = true;

        const size_t outBefore = n.outPins.size();
        RemovePinsByNameExcept(n.outPins, { "Body", "Completed", "Element" });
        if (n.outPins.size() != outBefore)
            changed = true;

        Pin* inPin = FindPinByName(n.inPins, "In");
        if (!inPin)
        {
            n.inPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "In",
                PinType::Flow,
                true
            ));
            inPin = &n.inPins.back();
            changed = true;
        }
        else if (inPin->type != PinType::Flow)
        {
            inPin->type = PinType::Flow;
            changed = true;
        }

        Pin* arrayPin = FindPinByName(n.inPins, "Array");
        if (!arrayPin)
        {
            n.inPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "Array",
                PinType::Array,
                true
            ));
            arrayPin = &n.inPins.back();
            changed = true;
        }
        else if (arrayPin->type != PinType::Array)
        {
            arrayPin->type = PinType::Array;
            changed = true;
        }

        Pin* bodyPin = FindPinByName(n.outPins, "Body");
        if (!bodyPin)
        {
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "Body",
                PinType::Flow,
                false
            ));
            bodyPin = &n.outPins.back();
            changed = true;
        }
        else if (bodyPin->type != PinType::Flow)
        {
            bodyPin->type = PinType::Flow;
            changed = true;
        }

        Pin* completedPin = FindPinByName(n.outPins, "Completed");
        if (!completedPin)
        {
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "Completed",
                PinType::Flow,
                false
            ));
            completedPin = &n.outPins.back();
            changed = true;
        }
        else if (completedPin->type != PinType::Flow)
        {
            completedPin->type = PinType::Flow;
            changed = true;
        }

        Pin* elementPin = FindPinByName(n.outPins, "Element");
        NodeField* elementTypeField = FindField(n.fields, "Element Type");
        if (!elementTypeField)
        {
            n.fields.push_back(NodeField{ "Element Type", PinType::String, "Any" });
            elementTypeField = &n.fields.back();
            changed = true;
        }
        else
        {
            if (elementTypeField->valueType != PinType::String)
            {
                elementTypeField->valueType = PinType::String;
                changed = true;
            }

            const PinType parsedType = VariableTypeFromString(elementTypeField->value);
            const std::string normalized = VariableTypeToString(parsedType);
            if (elementTypeField->value != normalized)
            {
                elementTypeField->value = normalized;
                changed = true;
            }
        }

        const PinType elementType = VariableTypeFromString(elementTypeField ? elementTypeField->value : "Any");
        if (!elementPin)
        {
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "Element",
                elementType,
                false
            ));
            elementPin = &n.outPins.back();
            changed = true;
        }
        else if (elementPin->type != elementType)
        {
            elementPin->type = elementType;
            changed = true;
        }

        NodeField* arrayField = FindField(n.fields, "Array");
        if (!arrayField)
        {
            n.fields.push_back(NodeField{ "Array", PinType::Array, "[]" });
            changed = true;
        }
        else
        {
            if (arrayField->valueType != PinType::Array)
            {
                arrayField->valueType = PinType::Array;
                changed = true;
            }

            const std::string before = arrayField->value;
            NormalizeValueForPinType(PinType::Array, arrayField->value);
            if (arrayField->value != before)
                changed = true;
        }
    }

    return changed;
}

bool RefreshDrawRectNodeLayout(GraphState& state)
{
    bool changed = false;

    auto& nodes = state.GetNodes();
    for (auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::DrawRect)
            continue;

        const size_t inBefore = n.inPins.size();
        RemovePinsByNameExcept(n.inPins, { "In", "X", "Y", "W", "H", "R", "G", "B" });
        if (n.inPins.size() != inBefore)
            changed = true;

        const size_t outBefore = n.outPins.size();
        RemovePinsByNameExcept(n.outPins, { "Out" });
        if (n.outPins.size() != outBefore)
            changed = true;

        auto ensureInput = [&](const char* name, PinType type)
        {
            Pin* pin = FindPinByName(n.inPins, name);
            if (!pin)
            {
                n.inPins.push_back(MakePin(
                    static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                    n.id,
                    n.nodeType,
                    name,
                    type,
                    true
                ));
                changed = true;
            }
            else if (pin->type != type)
            {
                pin->type = type;
                changed = true;
            }
        };

        ensureInput("In", PinType::Flow);
        ensureInput("X", PinType::Number);
        ensureInput("Y", PinType::Number);
        ensureInput("W", PinType::Number);
        ensureInput("H", PinType::Number);
        ensureInput("R", PinType::Number);
        ensureInput("G", PinType::Number);
        ensureInput("B", PinType::Number);

        Pin* outPin = FindPinByName(n.outPins, "Out");
        if (!outPin)
        {
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id,
                n.nodeType,
                "Out",
                PinType::Flow,
                false
            ));
            changed = true;
        }
        else if (outPin->type != PinType::Flow)
        {
            outPin->type = PinType::Flow;
            changed = true;
        }
    }

    return changed;
}

bool RefreshStructNodeLayouts(GraphState& state)
{
    bool changed = false;
    auto& nodes = state.GetNodes();

    struct StructDefEntry
    {
        std::vector<StructFieldDef> defs;
        std::string schemaText;
    };

    std::unordered_map<std::string, StructDefEntry> definitions;

    for (auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::StructDefine)
            continue;

        NodeField* nameField = EnsureField(n.fields, "Struct Name", PinType::String, "test", changed);
        NodeField* fieldsField = EnsureField(n.fields, "Fields", PinType::Array, "[]", changed);

        const std::vector<StructFieldDef> defs = ParseStructFieldDefs(fieldsField ? fieldsField->value : "[]");
        const std::string schemaName = (nameField && !nameField->value.empty()) ? nameField->value : "test";
        n.title = "Struct " + schemaName;

        RemovePinsByNameExcept(n.inPins, {});
        RemovePinsByNameExcept(n.outPins, { "Schema" });
        Pin* schemaPin = FindPinByName(n.outPins, "Schema");
        if (!schemaPin)
        {
            n.outPins.push_back(MakePin(static_cast<uint32_t>(state.GetIdGen().NewPin().Get()), n.id, n.nodeType, "Schema", PinType::Array, false));
            changed = true;
        }
        else if (schemaPin->type != PinType::Array)
        {
            schemaPin->type = PinType::Array;
            changed = true;
        }

        definitions[schemaName] = StructDefEntry{ defs, BuildStructFieldDefsString(defs) };
    }

    auto ensureSchemaFields = [&](VisualNode& n, const std::string& defaultName) -> std::pair<std::string, StructDefEntry>
    {
        NodeField* nameField = EnsureField(n.fields, "Struct Name", PinType::String, defaultName, changed);
        NodeField* schemaField = EnsureField(n.fields, "Schema Fields", PinType::Array, "[]", changed);

        std::string structName = (nameField && !nameField->value.empty()) ? nameField->value : defaultName;
        auto it = definitions.find(structName);
        if (it == definitions.end() && !definitions.empty())
        {
            structName = definitions.begin()->first;
            nameField->value = structName;
            changed = true;
            it = definitions.find(structName);
        }

        StructDefEntry entry;
        if (it != definitions.end())
            entry = it->second;

        const std::string targetSchema = entry.schemaText.empty() ? "[]" : entry.schemaText;
        if (schemaField->value != targetSchema)
        {
            schemaField->value = targetSchema;
            changed = true;
        }

        return { structName, entry };
    };

    for (auto& n : nodes)
    {
        if (!n.alive)
            continue;

        if (n.nodeType == NodeType::StructCreate)
        {
            auto [structName, entry] = ensureSchemaFields(n, "test");
            n.title = "Create " + structName;

            RemovePinsByNameExcept(n.inPins, { "Struct" });
            RemovePinsByNameExcept(n.outPins, { "Item" });

            {
                std::vector<NodeField> filteredFields;
                filteredFields.reserve(n.fields.size());
                for (const NodeField& field : n.fields)
                {
                    if (field.name == "Struct Name" || field.name == "Schema Fields")
                    {
                        filteredFields.push_back(field);
                        continue;
                    }

                    const bool existsInSchema = std::any_of(
                        entry.defs.begin(),
                        entry.defs.end(),
                        [&](const StructFieldDef& def) { return def.name == field.name; }
                    );

                    if (existsInSchema)
                        filteredFields.push_back(field);
                    else
                        changed = true;
                }

                if (filteredFields.size() != n.fields.size())
                    n.fields = std::move(filteredFields);
            }

            Pin* structPin = FindPinByName(n.inPins, "Struct");
            if (!structPin)
            {
                n.inPins.push_back(MakePin(static_cast<uint32_t>(state.GetIdGen().NewPin().Get()), n.id, n.nodeType, "Struct", PinType::String, true));
                changed = true;
            }

            for (const StructFieldDef& def : entry.defs)
            {
                Pin* fieldPin = FindPinByName(n.inPins, def.name.c_str());
                if (!fieldPin)
                {
                    n.inPins.push_back(MakePin(static_cast<uint32_t>(state.GetIdGen().NewPin().Get()), n.id, n.nodeType, def.name, def.type, true));
                    changed = true;
                }
                else if (fieldPin->type != def.type)
                {
                    fieldPin->type = def.type;
                    changed = true;
                }

                NodeField* valueField = EnsureField(n.fields, def.name, def.type, DefaultValueForPinType(def.type), changed);
                NormalizeValueForPinType(def.type, valueField->value);
            }

            Pin* itemPin = FindPinByName(n.outPins, "Item");
            if (!itemPin)
            {
                n.outPins.push_back(MakePin(static_cast<uint32_t>(state.GetIdGen().NewPin().Get()), n.id, n.nodeType, "Item", PinType::Array, false));
                changed = true;
            }
            else if (itemPin->type != PinType::Array)
            {
                itemPin->type = PinType::Array;
                changed = true;
            }
        }
        else if (n.nodeType == NodeType::StructGetField)
        {
            auto [structName, entry] = ensureSchemaFields(n, "test");
            NodeField* fieldField = EnsureField(n.fields, "Field", PinType::String, entry.defs.empty() ? "" : entry.defs.front().name, changed);
            if (!entry.defs.empty())
            {
                const bool exists = std::any_of(entry.defs.begin(), entry.defs.end(), [&](const StructFieldDef& def){ return def.name == fieldField->value; });
                if (!exists)
                {
                    fieldField->value = entry.defs.front().name;
                    changed = true;
                }
            }

            n.title = "Get " + structName + "." + fieldField->value;
            RemovePinsByNameExcept(n.inPins, { "Item" });
            RemovePinsByNameExcept(n.outPins, { "Value" });

            PinType valueType = PinType::Any;
            for (const StructFieldDef& def : entry.defs)
                if (def.name == fieldField->value) { valueType = def.type; break; }

            Pin* inPin = FindPinByName(n.inPins, "Item");
            if (!inPin)
            {
                n.inPins.push_back(MakePin(static_cast<uint32_t>(state.GetIdGen().NewPin().Get()), n.id, n.nodeType, "Item", PinType::Array, true));
                changed = true;
            }
            Pin* outPin = FindPinByName(n.outPins, "Value");
            if (!outPin)
            {
                n.outPins.push_back(MakePin(static_cast<uint32_t>(state.GetIdGen().NewPin().Get()), n.id, n.nodeType, "Value", valueType, false));
                changed = true;
            }
            else if (outPin->type != valueType)
            {
                outPin->type = valueType;
                changed = true;
            }
        }
        else if (n.nodeType == NodeType::StructSetField)
        {
            auto [structName, entry] = ensureSchemaFields(n, "test");
            NodeField* fieldField = EnsureField(n.fields, "Field", PinType::String, entry.defs.empty() ? "" : entry.defs.front().name, changed);
            if (!entry.defs.empty())
            {
                const bool exists = std::any_of(entry.defs.begin(), entry.defs.end(), [&](const StructFieldDef& def){ return def.name == fieldField->value; });
                if (!exists)
                {
                    fieldField->value = entry.defs.front().name;
                    changed = true;
                }
            }

            n.title = "Set " + structName + "." + fieldField->value;
            RemovePinsByNameExcept(n.inPins, { "Item", "Value" });
            RemovePinsByNameExcept(n.outPins, { "Out" });

            PinType valueType = PinType::Any;
            for (const StructFieldDef& def : entry.defs)
                if (def.name == fieldField->value) { valueType = def.type; break; }

            Pin* itemPin = FindPinByName(n.inPins, "Item");
            if (!itemPin)
            {
                n.inPins.push_back(MakePin(static_cast<uint32_t>(state.GetIdGen().NewPin().Get()), n.id, n.nodeType, "Item", PinType::Array, true));
                changed = true;
            }
            Pin* valuePin = FindPinByName(n.inPins, "Value");
            if (!valuePin)
            {
                n.inPins.push_back(MakePin(static_cast<uint32_t>(state.GetIdGen().NewPin().Get()), n.id, n.nodeType, "Value", valueType, true));
                changed = true;
            }
            else if (valuePin->type != valueType)
            {
                valuePin->type = valueType;
                changed = true;
            }
            Pin* outPin = FindPinByName(n.outPins, "Out");
            if (!outPin)
            {
                n.outPins.push_back(MakePin(static_cast<uint32_t>(state.GetIdGen().NewPin().Get()), n.id, n.nodeType, "Out", PinType::Array, false));
                changed = true;
            }
            else if (outPin->type != PinType::Array)
            {
                outPin->type = PinType::Array;
                changed = true;
            }
        }
        else if (n.nodeType == NodeType::StructDelete)
        {
            n.title = "Struct Delete";
            RemovePinsByNameExcept(n.inPins, { "In", "Array", "Id" });
            RemovePinsByNameExcept(n.outPins, { "Out" });

            // Backward compatibility: migrate legacy "Index" field to "Id".
            if (NodeField* legacyIndex = FindField(n.fields, "Index"))
            {
                NodeField* idField = FindField(n.fields, "Id");
                if (!idField)
                {
                    n.fields.push_back(NodeField{ "Id", PinType::Number, legacyIndex->value });
                    changed = true;
                }

                n.fields.erase(
                    std::remove_if(n.fields.begin(), n.fields.end(), [](const NodeField& f)
                    {
                        return f.name == "Index";
                    }),
                    n.fields.end()
                );
                changed = true;
            }

            EnsureField(n.fields, "Id", PinType::Number, "0", changed);

            Pin* idPin = FindPinByName(n.inPins, "Id");
            if (!idPin)
            {
                n.inPins.push_back(MakePin(
                    static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                    n.id,
                    n.nodeType,
                    "Id",
                    PinType::Number,
                    true
                ));
                changed = true;
            }
            else if (idPin->type != PinType::Number)
            {
                idPin->type = PinType::Number;
                changed = true;
            }
        }
    }

    return changed;
}

bool SyncLinkTypesAndPruneInvalid(GraphState& state)
{
    bool changed = false;
    auto& links = state.GetLinks();

    links.erase(
        std::remove_if(links.begin(), links.end(), [&](const Link& l)
        {
            const Pin* start = state.FindPin(l.startPinId);
            const Pin* end   = state.FindPin(l.endPinId);
            if (!start || !end)
            {
                changed = true;
                return true;
            }

            const Pin* outPin = !start->isInput ? start : end;
            const Pin* inPin  = start->isInput ? start : end;

            if (!Pin::CanConnect(*outPin, *inPin))
            {
                changed = true;
                return true;
            }

            return false;
        }),
        links.end()
    );

    for (Link& l : links)
    {
        const Pin* start = state.FindPin(l.startPinId);
        if (!start)
            continue;

        if (l.type != start->type)
        {
            l.type = start->type;
            changed = true;
        }
    }

    return changed;
}

void DisconnectNonAnyLinksForPins(GraphState& state, const std::vector<ed::PinId>& pinIds)
{
    if (pinIds.empty())
        return;

    auto touchesChangedPin = [&](const Link& l)
    {
        for (ed::PinId id : pinIds)
        {
            if (l.startPinId == id || l.endPinId == id)
                return true;
        }
        return false;
    };

    auto& links = state.GetLinks();
    const size_t before = links.size();

    links.erase(
        std::remove_if(links.begin(), links.end(), [&](const Link& l)
        {
            if (!touchesChangedPin(l))
                return false;

            return l.type != PinType::Any;
        }),
        links.end()
    );

    if (links.size() != before)
        state.MarkDirty();
}
}