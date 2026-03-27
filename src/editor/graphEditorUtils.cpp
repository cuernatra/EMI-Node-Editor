#include "graphEditorUtils.h"
#include "graphSerializer.h"
#include "../core/graph/link.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace
{
float ParseInspectorFloat(const std::string& s)
{
    if (s.empty())
        return 0.0f;

    try
    {
        return std::stof(s);
    }
    catch (...)
    {
        return 0.0f;
    }
}

bool ParseInspectorBool(const std::string& s)
{
    return s == "1" || s == "true" || s == "True";
}

PinType VariableTypeFromString(const std::string& typeName)
{
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

bool DrawInspectorField(NodeField& field)
{
    bool changed = false;
    ImGui::PushID(field.name.c_str());

    switch (field.valueType)
    {
        case PinType::Number:
        {
            float v = ParseInspectorFloat(field.value);
            if (ImGui::InputFloat(field.name.c_str(), &v, 0.0f, 0.0f, "%.3f"))
            {
                field.value = std::to_string(v);
                changed = true;
            }
            break;
        }

        case PinType::Boolean:
        {
            bool v = ParseInspectorBool(field.value);
            if (ImGui::Checkbox(field.name.c_str(), &v))
            {
                field.value = v ? "true" : "false";
                changed = true;
            }
            break;
        }

        case PinType::String:
        {
            if (field.name == "Op")
            {
                const char* items[] = { "+", "-", "*", "/", "==", "!=", "<", "<=", ">", ">=", "AND", "OR" };
                if (ImGui::BeginCombo("Op", field.value.c_str()))
                {
                    for (const char* item : items)
                    {
                        const bool isSelected = (field.value == item);
                        if (ImGui::Selectable(item, isSelected))
                        {
                            field.value = item;
                            changed = true;
                        }
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            else if (field.name == "Type")
            {
                const char* items[] = { "Number", "Boolean", "String", "Array" };
                if (ImGui::BeginCombo("Type", field.value.c_str()))
                {
                    for (const char* item : items)
                    {
                        const bool isSelected = (field.value == item);
                        if (ImGui::Selectable(item, isSelected))
                        {
                            field.value = item;
                            changed = true;
                        }
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            else
            {
                char buf[128] = {};
                std::strncpy(buf, field.value.c_str(), sizeof(buf) - 1);
                if (ImGui::InputText(field.name.c_str(), buf, sizeof(buf)))
                {
                    field.value = buf;
                    changed = true;
                }
            }
            break;
        }

        case PinType::Array:
        {
            char buf[128] = {};
            std::strncpy(buf, field.value.c_str(), sizeof(buf) - 1);
            if (ImGui::InputText(field.name.c_str(), buf, sizeof(buf)))
            {
                field.value = buf;
                changed = true;
            }
            break;
        }

        default:
            ImGui::LabelText(field.name.c_str(), "%s", field.value.c_str());
            break;
    }

    ImGui::PopID();
    return changed;
}

void DrawInspectorReadOnlyField(const NodeField& field)
{
    ImGui::TextUnformatted(field.name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", field.value.c_str());
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