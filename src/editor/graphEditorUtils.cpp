#include "graphEditorUtils.h"
#include "graphSerializer.h"
#include "../core/graph/link.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cctype>
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

static std::unordered_map<ImGuiID, bool> s_arrayItemOpenState;

bool TryParseDouble(const std::string& s, double& out);
bool ParseBoolLoose(const std::string& s);

enum class ArrayItemType
{
    Number,
    Boolean,
    String,
    Null,
    Array
};

ArrayItemType DetectArrayItemType(const std::string& raw)
{
    const std::string v = TrimCopy(raw);
    if (v == "null" || v == "Null")
        return ArrayItemType::Null;
    if (v == "true" || v == "True" || v == "false" || v == "False")
        return ArrayItemType::Boolean;
    if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') || (v.front() == '\'' && v.back() == '\'')))
        return ArrayItemType::String;
    if (v.size() >= 2 && v.front() == '[' && v.back() == ']')
        return ArrayItemType::Array;

    double number = 0.0;
    if (TryParseDouble(v, number))
        return ArrayItemType::Number;

    return ArrayItemType::String;
}

const char* ArrayItemTypeToLabel(ArrayItemType t)
{
    switch (t)
    {
        case ArrayItemType::Number:  return "Number";
        case ArrayItemType::Boolean: return "Boolean";
        case ArrayItemType::String:  return "String";
        case ArrayItemType::Null:    return "Null";
        case ArrayItemType::Array:   return "Array";
        default:                     return "String";
    }
}

std::string ArrayItemPayload(const std::string& raw, ArrayItemType type)
{
    std::string v = TrimCopy(raw);
    if (type == ArrayItemType::String)
    {
        if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') || (v.front() == '\'' && v.back() == '\'')))
            return v.substr(1, v.size() - 2);
        return v;
    }
    if (type == ArrayItemType::Null)
        return "";
    return v;
}

std::string BuildArrayItemFromPayload(ArrayItemType type, const std::string& payload)
{
    switch (type)
    {
        case ArrayItemType::Number:
        {
            double v = 0.0;
            if (TryParseDouble(payload, v))
                return std::to_string(v);
            return "0.0";
        }
        case ArrayItemType::Boolean:
            return ParseBoolLoose(payload) ? "true" : "false";
        case ArrayItemType::String:
            return "\"" + payload + "\"";
        case ArrayItemType::Null:
            return "null";
        case ArrayItemType::Array:
        {
            const std::string trimmed = TrimCopy(payload);
            if (trimmed.empty())
                return "[]";
            if (trimmed.front() == '[' && trimmed.back() == ']')
                return trimmed;
            return "[" + trimmed + "]";
        }
        default:
            return payload;
    }
}

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

bool DrawInspectorField(NodeField& field)
{
    bool changed = false;
    ImGui::PushID(field.name.c_str());

    auto beginInspectorRow = [&](const char* label)
    {
        const float rowStartX = ImGui::GetCursorPosX();
        constexpr float kLabelColumnWidth = 90.0f;

        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(rowStartX + kLabelColumnWidth);
        ImGui::SetNextItemWidth(-1.0f);
    };

    switch (field.valueType)
    {
        case PinType::Number:
        {
            beginInspectorRow(field.name.c_str());
            float v = ParseInspectorFloat(field.value);
            if (ImGui::InputFloat("##value", &v, 0.0f, 0.0f, "%.3f"))
            {
                field.value = std::to_string(v);
                changed = true;
            }
            break;
        }

        case PinType::Boolean:
        {
            beginInspectorRow(field.name.c_str());
            bool v = ParseInspectorBool(field.value);
            if (ImGui::Checkbox("##value", &v))
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
                beginInspectorRow(field.name.c_str());
                if (ImGui::BeginCombo("##op", field.value.c_str()))
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
                beginInspectorRow(field.name.c_str());
                if (ImGui::BeginCombo("##type", field.value.c_str()))
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
            else if (field.name == "Element Type")
            {
                const char* items[] = { "Any", "Number", "Boolean", "String", "Array" };
                beginInspectorRow(field.name.c_str());
                if (ImGui::BeginCombo("##elementType", field.value.c_str()))
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
                beginInspectorRow(field.name.c_str());
                char buf[128] = {};
                std::strncpy(buf, field.value.c_str(), sizeof(buf) - 1);
                if (ImGui::InputText("##value", buf, sizeof(buf)))
                {
                    field.value = buf;
                    changed = true;
                }
            }
            break;
        }

        case PinType::Array:
        {
            std::vector<std::string> items = ParseArrayItems(field.value);
            bool localChanged = false;
            int removeIndex = -1;

            ImGui::TextUnformatted(field.name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("[%d]", static_cast<int>(items.size()));

            // Bulk type switch for all elements in this specific array field.
            ImGui::SameLine();
            const bool hasItems = !items.empty();
            const ArrayItemType firstType = hasItems
                ? DetectArrayItemType(items.front())
                : ArrayItemType::Null;
            bool mixedTypes = false;
            for (size_t i = 1; i < items.size(); ++i)
            {
                if (DetectArrayItemType(items[i]) != firstType)
                {
                    mixedTypes = true;
                    break;
                }
            }

            const char* bulkPreview = hasItems
                ? (mixedTypes ? "Mixed" : ArrayItemTypeToLabel(firstType))
                : "(empty)";

            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::BeginCombo("##allItemType", bulkPreview))
            {
                const ArrayItemType allTypes[] = {
                    ArrayItemType::Number,
                    ArrayItemType::Boolean,
                    ArrayItemType::String,
                    ArrayItemType::Array,
                    ArrayItemType::Null
                };

                for (ArrayItemType t : allTypes)
                {
                    const bool selected = hasItems && !mixedTypes && (firstType == t);
                    if (ImGui::Selectable(ArrayItemTypeToLabel(t), selected) && hasItems)
                    {
                        for (size_t i = 0; i < items.size(); ++i)
                        {
                            const ArrayItemType currentType = DetectArrayItemType(items[i]);
                            const std::string payload = ArrayItemPayload(items[i], currentType);
                            items[i] = BuildArrayItemFromPayload(t, payload);
                        }
                        localChanged = true;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Indent(10.0f);
            for (int i = 0; i < static_cast<int>(items.size()); ++i)
            {
                ImGui::PushID(i);
                ImGui::TextDisabled("%d", i);
                ImGui::SameLine();

                ArrayItemType itemType = DetectArrayItemType(items[static_cast<size_t>(i)]);
                std::string payload = ArrayItemPayload(items[static_cast<size_t>(i)], itemType);
                std::vector<std::string> nestedItemsPreview;
                bool nestedOpen = false;
                ImGuiID nestedId = 0;
                if (itemType == ArrayItemType::Array)
                {
                    nestedItemsPreview = ParseArrayItems(items[static_cast<size_t>(i)]);
                    nestedId = ImGui::GetID("##nestedOpen");
                    nestedOpen = s_arrayItemOpenState[nestedId];
                }

                // Value editor first (wider), type selector second.
                ImGui::PushItemWidth(-150.0f);
                if (itemType == ArrayItemType::Boolean)
                {
                    bool b = ParseBoolLoose(payload);
                    if (ImGui::Checkbox("##itemBool", &b))
                    {
                        items[static_cast<size_t>(i)] = b ? "true" : "false";
                        localChanged = true;
                    }
                }
                else if (itemType == ArrayItemType::Null)
                {
                    ImGui::TextDisabled("null");
                }
                else if (itemType == ArrayItemType::Array)
                {
                    if (ImGui::ArrowButton("##nestedArrow", nestedOpen ? ImGuiDir_Down : ImGuiDir_Right))
                    {
                        nestedOpen = !nestedOpen;
                        s_arrayItemOpenState[nestedId] = nestedOpen;
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("Array [%d]", static_cast<int>(nestedItemsPreview.size()));
                }
                else
                {
                    char buf[192] = {};
                    std::strncpy(buf, payload.c_str(), sizeof(buf) - 1);
                    const char* itemWidgetId =
                        (itemType == ArrayItemType::Array) ? "##itemArray" :
                        (itemType == ArrayItemType::Number) ? "##itemNumber" :
                        "##itemString";

                    if (ImGui::InputText(itemWidgetId, buf, sizeof(buf)))
                    {
                        payload = buf;
                        items[static_cast<size_t>(i)] = BuildArrayItemFromPayload(itemType, payload);
                        localChanged = true;
                    }
                }
                ImGui::PopItemWidth();

                ImGui::SameLine();

                ImGui::SetNextItemWidth(96.0f);
                if (ImGui::BeginCombo("##itemType", ArrayItemTypeToLabel(itemType)))
                {
                    const ArrayItemType allTypes[] = {
                        ArrayItemType::Number,
                        ArrayItemType::Boolean,
                        ArrayItemType::String,
                        ArrayItemType::Array,
                        ArrayItemType::Null
                    };

                    for (ArrayItemType t : allTypes)
                    {
                        const bool selected = (itemType == t);
                        if (ImGui::Selectable(ArrayItemTypeToLabel(t), selected))
                        {
                            itemType = t;
                            items[static_cast<size_t>(i)] = BuildArrayItemFromPayload(itemType, payload);
                            localChanged = true;
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("-") && removeIndex < 0)
                    removeIndex = i;

                if (itemType == ArrayItemType::Array && nestedOpen)
                {
                    std::vector<std::string> nestedItems = ParseArrayItems(items[static_cast<size_t>(i)]);
                    bool nestedChanged = false;
                    int nestedRemove = -1;

                    ImGui::Indent(16.0f);
                    for (int ni = 0; ni < static_cast<int>(nestedItems.size()); ++ni)
                    {
                        ImGui::PushID(ni);
                        ImGui::TextDisabled("%d.%d", i, ni);
                        ImGui::SameLine();

                        ArrayItemType nestedType = DetectArrayItemType(nestedItems[static_cast<size_t>(ni)]);
                        std::string nestedPayload = ArrayItemPayload(nestedItems[static_cast<size_t>(ni)], nestedType);

                        ImGui::PushItemWidth(-150.0f);
                        if (nestedType == ArrayItemType::Boolean)
                        {
                            bool b = ParseBoolLoose(nestedPayload);
                            if (ImGui::Checkbox("##nestedBool", &b))
                            {
                                nestedItems[static_cast<size_t>(ni)] = b ? "true" : "false";
                                nestedChanged = true;
                            }
                        }
                        else if (nestedType == ArrayItemType::Null)
                        {
                            ImGui::TextDisabled("null");
                        }
                        else if (nestedType == ArrayItemType::Array)
                        {
                            const std::vector<std::string> nestedNested = ParseArrayItems(nestedItems[static_cast<size_t>(ni)]);
                            ImGui::TextDisabled("Array [%d]", static_cast<int>(nestedNested.size()));
                        }
                        else
                        {
                            char nbuf[192] = {};
                            std::strncpy(nbuf, nestedPayload.c_str(), sizeof(nbuf) - 1);
                            const char* nestedWidgetId =
                                (nestedType == ArrayItemType::Number) ? "##nestedNumber" :
                                "##nestedString";

                            if (ImGui::InputText(nestedWidgetId, nbuf, sizeof(nbuf)))
                            {
                                nestedPayload = nbuf;
                                nestedItems[static_cast<size_t>(ni)] = BuildArrayItemFromPayload(nestedType, nestedPayload);
                                nestedChanged = true;
                            }
                        }
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(96.0f);
                        if (ImGui::BeginCombo("##nestedType", ArrayItemTypeToLabel(nestedType)))
                        {
                            const ArrayItemType allTypes[] = {
                                ArrayItemType::Number,
                                ArrayItemType::Boolean,
                                ArrayItemType::String,
                                ArrayItemType::Array,
                                ArrayItemType::Null
                            };

                            for (ArrayItemType t : allTypes)
                            {
                                const bool selected = (nestedType == t);
                                if (ImGui::Selectable(ArrayItemTypeToLabel(t), selected))
                                {
                                    nestedType = t;
                                    nestedItems[static_cast<size_t>(ni)] = BuildArrayItemFromPayload(nestedType, nestedPayload);
                                    nestedChanged = true;
                                }
                                if (selected)
                                    ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }

                        ImGui::SameLine();
                        if (ImGui::SmallButton("-##nestedRemove") && nestedRemove < 0)
                            nestedRemove = ni;

                        ImGui::PopID();
                    }

                    if (nestedRemove >= 0)
                    {
                        nestedItems.erase(nestedItems.begin() + nestedRemove);
                        nestedChanged = true;
                    }

                    if (ImGui::SmallButton("+ Add nested"))
                    {
                        const ArrayItemType nextNestedType = nestedItems.empty()
                            ? ArrayItemType::Null
                            : DetectArrayItemType(nestedItems.back());
                        nestedItems.push_back(BuildArrayItemFromPayload(nextNestedType, ""));
                        nestedChanged = true;
                    }

                    ImGui::Unindent(16.0f);

                    if (nestedChanged)
                    {
                        items[static_cast<size_t>(i)] = BuildArrayString(nestedItems);
                        localChanged = true;
                    }
                }
                ImGui::PopID();
            }

            if (removeIndex >= 0)
            {
                items.erase(items.begin() + removeIndex);
                localChanged = true;
            }

            if (ImGui::SmallButton("+ Add element"))
            {
                // First element starts as null; subsequent elements inherit previous element type.
                const ArrayItemType nextType = items.empty()
                    ? ArrayItemType::Null
                    : DetectArrayItemType(items.back());
                items.push_back(BuildArrayItemFromPayload(nextType, ""));
                localChanged = true;
            }

            ImGui::Unindent(10.0f);

            if (localChanged)
            {
                field.value = BuildArrayString(items);
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
    const float rowStartX = ImGui::GetCursorPosX();
    constexpr float kLabelColumnWidth = 90.0f;

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(field.name.c_str());
    ImGui::SameLine(rowStartX + kLabelColumnWidth);

    ImGui::SetNextItemWidth(-1.0f);
    char buf[256] = {};
    std::strncpy(buf, field.value.c_str(), sizeof(buf) - 1);
    ImGui::InputText("##readonly", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
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

    // ???????????????????????????????????????????????????????????????????????????????????????
    for (const auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Function)
            continue;

        for (int i = 0; ; ++i)
        {
            const std::string paramKey = "Param" + std::to_string(i);
            const NodeField* paramField = FindField(
                const_cast<std::vector<NodeField>&>(n.fields), paramKey.c_str());
            if (!paramField || paramField->value.empty())
                break;

            definitions[paramField->value] = VariableDef{
                PinType::Number,
                "Number"
            };
        }
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

bool RefreshCallFunctionNodeLayout(GraphState& state)
{
    bool changed = false;

    auto& nodes = state.GetNodes();

    // Kerää kaikki Function-nodet ja niiden parametrit
    struct FunctionDef
    {
        std::vector<std::string> params;
    };
    std::unordered_map<std::string, FunctionDef> functionDefs;

    for (const auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Function)
            continue;

        const NodeField* nameField = FindField(n.fields, "Name");
        if (!nameField || nameField->value.empty())
            continue;

        FunctionDef def;
        for (int i = 0; ; ++i)
        {
            const std::string paramKey = "Param" + std::to_string(i);
            const NodeField* paramField = FindField(const_cast<std::vector<NodeField>&>(n.fields), paramKey.c_str());
            if (!paramField || paramField->value.empty())
                break;
            def.params.push_back(paramField->value);
        }
        functionDefs[nameField->value] = def;
    }

    // Päivitä CallFunction-nodet
    for (auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::CallFunction)
            continue;

        const NodeField* nameField = FindField(const_cast<std::vector<NodeField>&>(n.fields), "Name");
        const std::string funcName = nameField ? nameField->value : "";

        // Varmista In, Out, Result pinit
        Pin* inPin = FindPinByName(n.inPins, "In");
        if (!inPin)
        {
            n.inPins.insert(n.inPins.begin(), MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id, n.nodeType, "In", PinType::Flow, true));
            changed = true;
        }

        Pin* outPin = FindPinByName(n.outPins, "Out");
        if (!outPin)
        {
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id, n.nodeType, "Out", PinType::Flow, false));
            changed = true;
        }

        Pin* resultPin = FindPinByName(n.outPins, "Result");
        if (!resultPin)
        {
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id, n.nodeType, "Result", PinType::Any, false));
            changed = true;
        }

        // Päivitä parametripinit funktion määrittelystä
        auto it = functionDefs.find(funcName);
        if (it != functionDefs.end())
        {
            const auto& params = it->second.params;

            // Poista vanhat parametripinit
            std::vector<std::string> keepPins = { "In" };
            for (const auto& p : params)
                keepPins.push_back(p);

            const size_t inBefore = n.inPins.size();
            RemovePinsByNameExcept(n.inPins, keepPins);
            if (n.inPins.size() != inBefore)
                changed = true;

            // Lisää puuttuvat parametripinit
            for (const auto& paramName : params)
            {
                Pin* paramPin = FindPinByName(n.inPins, paramName.c_str());
                if (!paramPin)
                {
                    n.inPins.push_back(MakePin(
                        static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                        n.id, n.nodeType, paramName, PinType::Any, true));
                    changed = true;
                }
            }
        }
    }

    return changed;
}

//??????????????????????????????????????????????????????????????????????????????????????????????
bool RefreshFunctionNodeLayout(GraphState& state)
{
    bool changed = false;
    auto& nodes = state.GetNodes();

    for (auto& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Function)
            continue;

        // Varmista vain In ja Body pinit — ei parametripineja
        Pin* inPin = FindPinByName(n.inPins, "In");
        if (!inPin)
        {
            n.inPins.insert(n.inPins.begin(), MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id, n.nodeType, "In", PinType::Flow, true));
            changed = true;
        }

        // Poista kaikki muut input-pinit paitsi In
        const size_t inBefore = n.inPins.size();
        RemovePinsByNameExcept(n.inPins, { "In" });
        if (n.inPins.size() != inBefore)
            changed = true;

        Pin* bodyPin = FindPinByName(n.outPins, "Body");
        if (!bodyPin)
        {
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(state.GetIdGen().NewPin().Get()),
                n.id, n.nodeType, "Body", PinType::Flow, false));
            changed = true;
        }

        // Varmista vain Body output-pinissa
        const size_t outBefore = n.outPins.size();
        RemovePinsByNameExcept(n.outPins, { "Body" });
        if (n.outPins.size() != outBefore)
            changed = true;
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