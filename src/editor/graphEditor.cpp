#include "graphEditor.h"
#include "graphState.h"
#include "graphSerializer.h"
#include "../core/graph/visualNode.h"
#include "../core/graph/link.h"
#include "../core/registry/nodeFactory.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>

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

ImVec2 SnapToNodeGrid(const ImVec2& pos)
{
    // Keep spawn positions on the same grid that node dragging uses.
    // This avoids nodes spawning "between" allowed drag positions.
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

Pin* FindPinByName(std::vector<Pin>& pins, const char* name)
{
    for (Pin& p : pins)
        if (p.name == name)
            return &p;
    return nullptr;
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
                const char* items[] = { "+", "-", "*", "/", "==", "!=", "<", "<=", ">", ">=", "AND", "OR", "NOT" };
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
            setPin = FindPinByName(n.inPins, "Set"); // backward compatibility
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
                // Match Constant-node behavior: when type changes, reset default value.
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
        const std::string varName = nameField ? nameField->value : "myVar";

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

            const auto it = definitions.find(varName);
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

            // Keep white (Any) links, remove typed links when pin type changes.
            return l.type != PinType::Any;
        }),
        links.end()
    );

    if (links.size() != before)
        state.MarkDirty();
}
}

GraphEditor::GraphEditor(ed::EditorContext* ctx, GraphState& state)
    : m_ctx(ctx), m_state(state)
{
}

GraphEditor::~GraphEditor()
{
}

void GraphEditor::Draw()
{
    ed::SetCurrentEditor(m_ctx);

    DrawNodeCanvas();
    DrawContextMenus();

    ed::SetCurrentEditor(nullptr);
}

void GraphEditor::DrawNodeCanvas()
{
    ed::Begin("MainGraph");

    ed::Suspend();

    // Handle drag-drop spawn
    const ImGuiPayload* pl = ImGui::GetDragDropPayload();
    if (pl && pl->IsDataType("SPAWN_NODE"))
    {
        ImVec2 dropSize = ImGui::GetContentRegionAvail();
        if (dropSize.x < 1) dropSize.x = 1;
        if (dropSize.y < 1) dropSize.y = 1;

        ImGui::InvisibleButton("##editor_drop_area", dropSize);

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SPAWN_NODE"))
            {
                const NodeSpawnPayload* spawnData =
                    static_cast<const NodeSpawnPayload*>(payload->Data);

                ImVec2 canvasPos = SnapToNodeGrid(ed::ScreenToCanvas(ImGui::GetMousePos()));

                NodeType type = NodeType::Unknown;
                std::string variableVariant;
                ParseSpawnPayloadTitle(spawnData->title, type, variableVariant);

                if (type != NodeType::Unknown)
                {
                    VisualNode newNode = CreateNodeFromType(type, m_state.GetIdGen(), canvasPos);

                    if (type == NodeType::Variable && !variableVariant.empty())
                    {
                        if (NodeField* variant = FindField(newNode.fields, "Variant"))
                            variant->value = (variableVariant == "Get") ? "Get" : "Set";
                    }

                    m_state.AddNode(newNode);
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    ed::Resume();

    if (!m_initialAutoStartHandled)
    {
        if (!m_state.HasAliveNodes())
        {
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImVec2 windowSize = ImGui::GetWindowSize();
            const ImVec2 targetScreenPos(
                windowPos.x + windowSize.x * 0.38f,
                windowPos.y + windowSize.y * 0.50f
            );

            const ImVec2 targetCanvasPos = SnapToNodeGrid(ed::ScreenToCanvas(targetScreenPos));
            m_state.AddNode(CreateNodeFromType(NodeType::Start, m_state.GetIdGen(), targetCanvasPos));
        }

        m_initialAutoStartHandled = true;
    }

    // Draw all nodes
    bool anyChanged = false;
    for (auto& n : m_state.GetNodes())
    {
        if (!n.alive)
            continue;

        std::unordered_map<uintptr_t, PinType> pinTypesBefore;
        pinTypesBefore.reserve(n.inPins.size() + n.outPins.size());
        for (const Pin& p : n.inPins)
            pinTypesBefore.emplace(p.id.Get(), p.type);
        for (const Pin& p : n.outPins)
            pinTypesBefore.emplace(p.id.Get(), p.type);

        anyChanged |= DrawVisualNode(n, &m_state.GetIdGen(), &m_state.GetNodes(), &m_state.GetLinks());

        std::vector<ed::PinId> changedPins;
        for (const Pin& p : n.inPins)
        {
            auto it = pinTypesBefore.find(p.id.Get());
            if (it != pinTypesBefore.end() && it->second != p.type)
                changedPins.push_back(p.id);
        }
        for (const Pin& p : n.outPins)
        {
            auto it = pinTypesBefore.find(p.id.Get());
            if (it != pinTypesBefore.end() && it->second != p.type)
                changedPins.push_back(p.id);
        }

        DisconnectNonAnyLinksForPins(m_state, changedPins);
    }

    if (anyChanged) 
    {
        m_state.MarkDirty();
    }

    // Draw all links
    DrawLinks(m_state.GetLinks());

    // Handle creation and deletion
    if (ed::BeginDelete())
    {
        ed::LinkId linkId;
        DeleteLinks(linkId);

        ed::NodeId nodeId;
        DeleteNodes(nodeId);
    }
    ed::EndDelete();

    if (ed::BeginCreate())
        CreateNewLink();
    ed::EndCreate();

    // UX rule:
    // If user clicks the same already-selected single node again,
    // deselect it (so inspector closes instead of becoming dim/stacked).
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const ed::NodeId hoveredNode = ed::GetHoveredNode();
        if (hoveredNode.Get() != 0 && ed::IsNodeSelected(hoveredNode))
        {
            const int selectedCount = ed::GetSelectedObjectCount();
            if (selectedCount > 0)
            {
                std::vector<ed::NodeId> selectedNodes(static_cast<size_t>(selectedCount));
                const int selectedNodeCount = ed::GetSelectedNodes(selectedNodes.data(), selectedCount);
                if (selectedNodeCount == 1 && selectedNodes.front() == hoveredNode)
                {
                    ed::DeselectNode(hoveredNode);
                }
            }
        }
    }

    const bool variableTypesChanged = RefreshVariableNodeTypes(m_state);
    const bool loopLayoutChanged = RefreshLoopNodeLayout(m_state);
    const bool outputInputTypesChanged = RefreshOutputNodeInputTypes(m_state);
    const bool linksChanged = SyncLinkTypesAndPruneInvalid(m_state);
    if (variableTypesChanged || loopLayoutChanged || outputInputTypesChanged || linksChanged)
        m_state.MarkDirty();

    ed::End();
}



void GraphEditor::DrawContextMenus()
{
    // Placeholder for future context menu logic
}

void GraphEditor::DrawInspectorPanel()
{
    uintptr_t selectedNodeRawId = 0;
    if (!TryGetSingleSelectedNodeId(selectedNodeRawId))
        return;

    const ed::NodeId selectedNodeId(selectedNodeRawId);
    VisualNode* selectedNode = nullptr;
    for (auto& node : m_state.GetNodes())
    {
        if (!node.alive)
            continue;

        if (node.id == selectedNodeId)
        {
            selectedNode = &node;
            break;
        }
    }

    if (!selectedNode)
        return;

    const ImVec2 nodePos = ed::GetNodePosition(selectedNode->id);

    ImGui::Text("Node ID: %llu", static_cast<unsigned long long>(selectedNode->id.Get()));
    ImGui::Text("Type: %s", NodeTypeToString(selectedNode->nodeType));
    ImGui::Text("Position: (%.1f, %.1f)", nodePos.x, nodePos.y);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("VALUES");
    ImGui::Spacing();

    bool fieldsChanged = false;
    if (selectedNode->fields.empty())
    {
        ImGui::TextUnformatted("No values.");
    }
    else
    {
        // Special inspector behavior for Get Variable:
        // - allow selecting only an existing Set variable by name
        // - keep type/default read-only
        // - no other editable fields
        if (selectedNode->nodeType == NodeType::Variable)
        {
            NodeField* variantField = FindField(selectedNode->fields, "Variant");
            NodeField* nameField = FindField(selectedNode->fields, "Name");
            NodeField* typeField = FindField(selectedNode->fields, "Type");

            const bool isGet = variantField && variantField->value == "Get";
            bool defaultConnected = false;
            if (!isGet)
            {
                Pin* defaultPin = FindPinByName(selectedNode->inPins, "Default");
                if (!defaultPin)
                    defaultPin = FindPinByName(selectedNode->inPins, "Set"); // backward compatibility

                if (defaultPin)
                {
                    for (const Link& l : m_state.GetLinks())
                    {
                        if (l.alive && l.endPinId == defaultPin->id)
                        {
                            defaultConnected = true;
                            break;
                        }
                    }
                }
            }

            ImGui::PushID(static_cast<int>(selectedNode->id.Get()));

            if (isGet)
            {
                // Get node inspector:
                // - Variable drop-down from existing Set variables
                std::vector<std::string> setVariableNames;

                for (const auto& node : m_state.GetNodes())
                {
                    if (!node.alive || node.nodeType != NodeType::Variable)
                        continue;

                    const NodeField* nVariant = FindField(node.fields, "Variant");
                    if (!nVariant || nVariant->value != "Set")
                        continue;

                    const NodeField* nName = FindField(node.fields, "Name");
                    const std::string varName = nName ? nName->value : "myVar";

                    if (std::find(setVariableNames.begin(), setVariableNames.end(), varName) == setVariableNames.end())
                        setVariableNames.push_back(varName);
                }

                if (nameField)
                {
                    if (setVariableNames.empty())
                    {
                        ImGui::TextUnformatted("Variable");
                        ImGui::SameLine();
                        ImGui::TextDisabled("(no Set variables)");
                    }
                    else
                    {
                        if (std::find(setVariableNames.begin(), setVariableNames.end(), nameField->value) == setVariableNames.end())
                        {
                            nameField->value = setVariableNames.front();
                            fieldsChanged = true;
                        }

                        if (ImGui::BeginCombo("Variable", nameField->value.c_str()))
                        {
                            for (const auto& varName : setVariableNames)
                            {
                                const bool selected = (nameField->value == varName);
                                if (ImGui::Selectable(varName.c_str(), selected))
                                {
                                    nameField->value = varName;
                                    fieldsChanged = true;
                                }
                                if (selected)
                                    ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    }
                }

                if (typeField)
                {
                    DrawInspectorReadOnlyField(*typeField);
                }
            }
            else
            {
                for (NodeField& field : selectedNode->fields)
                {
                    if (&field == variantField)
                        continue;

                    if (field.name == "Default" && defaultConnected)
                    {
                        DrawInspectorReadOnlyField(field);
                        continue;
                    }

                    fieldsChanged |= DrawInspectorField(field);
                }
            }

            ImGui::PopID();
        }
        else if (selectedNode->nodeType == NodeType::Loop)
        {
            auto isInputPinConnected = [&](const char* pinName) -> bool
            {
                Pin* targetPin = FindPinByName(selectedNode->inPins, pinName);
                if (!targetPin)
                    return false;

                for (const Link& l : m_state.GetLinks())
                {
                    if (l.alive && l.endPinId == targetPin->id)
                        return true;
                }
                return false;
            };

            ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
            for (NodeField& field : selectedNode->fields)
            {
                if ((field.name == "Start" || field.name == "Count")
                    && isInputPinConnected(field.name.c_str()))
                {
                    DrawInspectorReadOnlyField(field);
                    continue;
                }

                fieldsChanged |= DrawInspectorField(field);
            }
            ImGui::PopID();
        }
        else
        {
            ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
            for (NodeField& field : selectedNode->fields)
                fieldsChanged |= DrawInspectorField(field);
            ImGui::PopID();
        }
    }

    if (fieldsChanged)
    {
        GraphSerializer::ApplyConstantTypeFromFields(*selectedNode, /*resetValueOnTypeChange=*/true);

        RefreshVariableNodeTypes(m_state);
        RefreshLoopNodeLayout(m_state);
        SyncLinkTypesAndPruneInvalid(m_state);
        m_state.MarkDirty();
    }
}

bool GraphEditor::HasSelectedNode() const
{
    uintptr_t selectedNodeId = 0;
    return TryGetSingleSelectedNodeId(selectedNodeId);
}

void GraphEditor::HandleDeleteShortcutFallback()
{
    const int deleteKeyIndex = ImGui::GetKeyIndex(ImGuiKey_Delete);
    const int backspaceKeyIndex = ImGui::GetKeyIndex(ImGuiKey_Backspace);

    const bool deletePressed =
        (deleteKeyIndex >= 0 && ImGui::IsKeyPressed(deleteKeyIndex, false)) ||
        (backspaceKeyIndex >= 0 && ImGui::IsKeyPressed(backspaceKeyIndex, false));

    // Block only when user is actively editing text.
    if (!deletePressed || ImGui::GetIO().WantTextInput)
        return;

    bool anyDeleted = false;

    const int selectedCount = ed::GetSelectedObjectCount();
    if (selectedCount > 0)
    {
        std::vector<ed::LinkId> selectedLinks(static_cast<size_t>(selectedCount));
        const int selectedLinkCount = ed::GetSelectedLinks(selectedLinks.data(), selectedCount);
        for (int i = 0; i < selectedLinkCount; ++i)
        {
            m_state.DeleteLink(selectedLinks[static_cast<size_t>(i)]);
            anyDeleted = true;
        }

        std::vector<ed::NodeId> selectedNodes(static_cast<size_t>(selectedCount));
        const int selectedNodeCount = ed::GetSelectedNodes(selectedNodes.data(), selectedCount);
        for (int i = 0; i < selectedNodeCount; ++i)
        {
            m_state.DeleteNode(selectedNodes[static_cast<size_t>(i)]);
            anyDeleted = true;
        }
    }

    if (anyDeleted)
    {
        RefreshVariableNodeTypes(m_state);
        SyncLinkTypesAndPruneInvalid(m_state);
        m_state.MarkDirty();
    }
}

bool GraphEditor::TryGetSingleSelectedNodeId(uintptr_t& outId) const
{
    outId = 0;

    const int selectedCount = ed::GetSelectedObjectCount();
    if (selectedCount <= 0)
        return false;

    std::vector<ed::NodeId> selectedNodes(static_cast<size_t>(selectedCount));
    const int selectedNodeCount = ed::GetSelectedNodes(selectedNodes.data(), selectedCount);
    // Inspector is shown only when exactly one node is selected.
    if (selectedNodeCount != 1)
        return false;

    const ed::NodeId selectedNodeId = selectedNodes.front();
    for (const auto& node : m_state.GetNodes())
    {
        if (node.alive && node.id == selectedNodeId)
        {
            outId = static_cast<uintptr_t>(selectedNodeId.Get());
            return true;
        }
    }

    return false;
}

void GraphEditor::CreateNewLink()
{
    ed::PinId startPinId, endPinId;
    if (!ed::QueryNewLink(&startPinId, &endPinId))
        return;

    const Pin* startPin = m_state.FindPin(startPinId);
    const Pin* endPin   = m_state.FindPin(endPinId);

    if (!startPin || !endPin || !Pin::CanConnect(*startPin, *endPin))
    {
        ed::RejectNewItem();
        return;
    }

    if (ed::AcceptNewItem())
    {
        ed::PinId outPinId = !startPin->isInput ? startPinId : endPinId;
        ed::PinId inPinId  = startPin->isInput ? startPinId : endPinId;
        const Pin* outPin = !startPin->isInput ? startPin : endPin;
        const Pin* inPin  = startPin->isInput ? startPin : endPin;

        std::vector<Link> filteredLinks;
        filteredLinks.reserve(m_state.GetLinks().size());

        const bool isFlowLink = (outPin->type == PinType::Flow);

        // Connection policy:
        // - Flow (white): one outgoing per output pin, allow stacking into same input
        //   (including Debug Print flow input).
        // - Non-flow (typed): allow fan-out from output pin, but keep each input pin single-source.
        const bool replaceExistingFromOutput = isFlowLink;
        const bool replaceExistingToInput = !isFlowLink;

        for (const Link& l : m_state.GetLinks())
        {
            if (!l.alive)
                continue;

            if (replaceExistingFromOutput && l.startPinId == outPinId)
                continue;

            if (replaceExistingToInput && l.endPinId == inPinId)
                continue;

            filteredLinks.push_back(l);
        }

        if (WouldCreateCycle(filteredLinks, outPinId, inPinId))
        {
            ed::RejectNewItem();
            return;
        }

        auto& links = m_state.GetLinks();
        links.erase(
            std::remove_if(links.begin(), links.end(), [&](const Link& l)
            {
                if (!l.alive)
                    return true;

                if (replaceExistingFromOutput && l.startPinId == outPinId)
                    return true;

                if (replaceExistingToInput && l.endPinId == inPinId)
                    return true;

                return false;
            }),
            links.end()
        );

        Link lnk;
        lnk.id         = m_state.GetIdGen().NewLink();
        lnk.startPinId = outPinId;
        lnk.endPinId   = inPinId;
        lnk.type       = outPin->type;
        m_state.AddLink(lnk);

        const bool variableTypesChanged = RefreshVariableNodeTypes(m_state);
        const bool linksChanged = SyncLinkTypesAndPruneInvalid(m_state);
        if (variableTypesChanged || linksChanged)
            m_state.MarkDirty();
    }
}

void GraphEditor::DeleteNodes(ed::NodeId nodeId)
{
    bool anyDeleted = false;
    while (ed::QueryDeletedNode(&nodeId))
    {
        if (!ed::AcceptDeletedItem()) continue;
        m_state.DeleteNode(nodeId);
        anyDeleted = true;
    }

    if (anyDeleted)
    {
        const bool variableTypesChanged = RefreshVariableNodeTypes(m_state);
        const bool linksChanged = SyncLinkTypesAndPruneInvalid(m_state);
        if (variableTypesChanged || linksChanged)
            m_state.MarkDirty();
    }
}

void GraphEditor::DeleteLinks(ed::LinkId linkId)
{
    bool anyDeleted = false;
    while (ed::QueryDeletedLink(&linkId))
    {
        if (!ed::AcceptDeletedItem()) continue;
        m_state.DeleteLink(linkId);
        anyDeleted = true;
    }

    if (anyDeleted)
    {
        const bool variableTypesChanged = RefreshVariableNodeTypes(m_state);
        const bool linksChanged = SyncLinkTypesAndPruneInvalid(m_state);
        if (variableTypesChanged || linksChanged)
            m_state.MarkDirty();
    }
}
