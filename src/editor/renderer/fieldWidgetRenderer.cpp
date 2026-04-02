#include "fieldWidgetRenderer.h"

#include "imgui.h"
#include "imgui_node_editor.h"
#include "ui/theme.h"

#include <cstdlib>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <array>
#include <unordered_map>
#include <vector>

namespace ed = ax::NodeEditor;

namespace
{
constexpr float kCompactFieldWidth = 78.0f;
constexpr float kInspectorLabelColumnWidth = 90.0f;
}

static void BeginInspectorRow(const char* label)
{
    const float rowStartX = ImGui::GetCursorPosX();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(rowStartX + kInspectorLabelColumnWidth);
    ImGui::SetNextItemWidth(-1.0f);
}

static float ParseFloat(const std::string& s)
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

static bool ParseBool(const std::string& s)
{
    return s == "1" || s == "true" || s == "True";
}

static std::string TrimArrayToken(const std::string& s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

static std::vector<std::string> ParseArrayItems(const std::string& text)
{
    std::string src = TrimArrayToken(text);
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

    auto pushCurrent = [&]()
    {
        std::string item = TrimArrayToken(current);
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

static std::string BuildArrayString(const std::vector<std::string>& items)
{
    std::string out = "[";
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (i != 0)
            out += ", ";
        out += TrimArrayToken(items[i]);
    }
    out += "]";
    return out;
}

static bool TryParseDouble(const std::string& s, double& out)
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

static bool ParseBoolLoose(const std::string& s)
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

enum class ArrayItemType
{
    Number,
    Boolean,
    String,
    Null,
    Array
};

static ArrayItemType DetectArrayItemType(const std::string& raw)
{
    const std::string v = TrimArrayToken(raw);
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

static const char* ArrayItemTypeToLabel(ArrayItemType t)
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

static ArrayItemType ArrayItemTypeFromLabel(const std::string& label)
{
    if (label == "Number")  return ArrayItemType::Number;
    if (label == "Boolean") return ArrayItemType::Boolean;
    if (label == "String")  return ArrayItemType::String;
    if (label == "Array")   return ArrayItemType::Array;
    return ArrayItemType::Null;
}

static std::string ArrayItemPayload(const std::string& raw, ArrayItemType type)
{
    std::string v = TrimArrayToken(raw);
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

static std::string BuildArrayItemFromPayload(ArrayItemType type, const std::string& payload)
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
            const std::string trimmed = TrimArrayToken(payload);
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

static std::unordered_map<ImGuiID, bool> s_wasActive;
static std::unordered_map<ImGuiID, bool> s_arrayOpenState;
static std::unordered_map<ImGuiID, bool> s_arrayNestedOpenState;

static void HandleShortcutToggle(const char* widgetId)
{
    ImGuiID id = ImGui::GetID(widgetId);
    bool isActive = ImGui::IsItemActive();

    bool wasActive = false;
    auto it = s_wasActive.find(id);
    if (it != s_wasActive.end())
        wasActive = it->second;

    if (isActive && !wasActive)
    {
        ed::EnableShortcuts(false);
        s_wasActive[id] = true;
    }
    else if (!isActive && wasActive)
    {
        ed::EnableShortcuts(true);
        s_wasActive[id] = false;
    }
}

static bool OpPopupCombo(const char* id, std::string& value, const char** items, float width = 100.0f)
{
    bool changed = false;

    ImGui::PushID(id);

    bool open = false;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));

    if (ImGui::Button(value.c_str(), ImVec2(width, 0.0f)))
        open = true;

    ImVec2 leftMin  = ImGui::GetItemRectMin();
    ImVec2 rightMax = ImGui::GetItemRectMax();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    float arrowX = rightMax.x - ImGui::GetFrameHeight() * 0.55f;
    float arrowY = (leftMin.y + rightMax.y) * 0.5f;

    draw->AddTriangleFilled(
        ImVec2(arrowX - 4.0f, arrowY - 2.0f),
        ImVec2(arrowX + 4.0f, arrowY - 2.0f),
        ImVec2(arrowX,        arrowY + 3.0f),
        ImGui::GetColorU32(colors::textPrimary)
    );

    ImGui::PopStyleVar();

    if (open)
        ImGui::OpenPopup("##popup");

    ed::Suspend();

    ImGui::SetNextWindowPos({ImGui::GetMousePos().x, ImGui::GetMousePos().y}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(40.0f, 0.0f), ImVec2(10000.0f, 10000.0f));

    if (ImGui::BeginPopup("##popup",
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings))
    {
        for (int i = 0; items[i]; ++i)
        {
            const bool selected = (value == items[i]);
            if (ImGui::Selectable(items[i], selected))
            {
                value = items[i];
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndPopup();
    }

    ed::Resume();

    ImGui::PopID();
    return changed;
}

bool DrawField(NodeField& field)
{
	return DrawField(field, FieldWidgetLayout::Compact);
}

void DrawFieldReadOnly(const NodeField& field, FieldWidgetLayout layout)
{
	if (layout == FieldWidgetLayout::Inspector)
	{
		BeginInspectorRow(field.name.c_str());
		char buf[256] = {};
		std::strncpy(buf, field.value.c_str(), sizeof(buf) - 1);
		ImGui::InputText("##readonly", buf, sizeof(buf), ImGuiInputTextFlags_ReadOnly);
		return;
	}

	ImGui::LabelText(field.name.c_str(), "%s", field.value.c_str());
}

bool DrawField(NodeField& field, FieldWidgetLayout layout)
{
    bool changed = false;

    ImGui::PushID(field.name.c_str());

    switch (field.valueType)
    {
        case PinType::Number:
        {
            if (layout == FieldWidgetLayout::Inspector)
            {
                BeginInspectorRow(field.name.c_str());

                float v = ParseFloat(field.value);
                if (ImGui::InputFloat("##value", &v, 0.0f, 0.0f, "%.3f"))
                {
                    field.value = std::to_string(v);
                    changed = true;
                }
                HandleShortcutToggle("##value");
                break;
            }

            ImGui::Text("%s", field.name.c_str());
            ImGui::SameLine();
            ImGui::PushItemWidth(kCompactFieldWidth);

            float v = ParseFloat(field.value);

            if (ImGui::InputFloat("##value", &v, 0.0f, 0.0f, "%.3f"))
            {
                field.value = std::to_string(v);
                changed = true;
            }
            HandleShortcutToggle("##value");

            ImGui::PopItemWidth();
            break;
        }

        case PinType::Boolean:
        {
            bool v = ParseBool(field.value);

            if (layout == FieldWidgetLayout::Inspector)
            {
                BeginInspectorRow(field.name.c_str());
                if (ImGui::Checkbox("##value", &v))
                {
                    field.value = v ? "true" : "false";
                    changed = true;
                }
                HandleShortcutToggle("##value");
                break;
            }

            if (ImGui::Checkbox(field.name.c_str(), &v))
            {
                field.value = v ? "true" : "false";
                changed = true;
            }
            HandleShortcutToggle(field.name.c_str());

            break;
        }

        case PinType::String:
        {
            if (field.name == "Op")
            {
                if (layout == FieldWidgetLayout::Inspector)
                {
                    static const char* kItems[] = {
                        "+", "-", "*", "/",
                        "==", "!=", "<", "<=", ">", ">=",
                        "AND", "OR",
                        nullptr
                    };

                    BeginInspectorRow(field.name.c_str());
                    if (ImGui::BeginCombo("##op", field.value.c_str()))
                    {
                        for (int i = 0; kItems[i]; ++i)
                        {
                            const bool isSelected = (field.value == kItems[i]);
                            if (ImGui::Selectable(kItems[i], isSelected))
                            {
                                field.value = kItems[i];
                                changed = true;
                            }
                            if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    HandleShortcutToggle("##op");
                }
                else
                {
                    static const char* kArith[] = { "+", "-", "*", "/", nullptr };
                    static const char* kCmp[]   = { "==", "!=", "<", "<=", ">", ">=", nullptr };
                    static const char* kLogic[] = { "AND", "OR", nullptr };

                    const char** items = kArith;

                    if (field.value == "==" || field.value == "!=" ||
                        field.value == "<"  || field.value == "<=" ||
                        field.value == ">"  || field.value == ">=")
                    {
                        items = kCmp;
                    }
                    else if (field.value == "AND" || field.value == "OR")
                    {
                        items = kLogic;
                    }

                    if (OpPopupCombo("##OpCombo", field.value, items, 100.0f))
                        changed = true;
                }
            }
            else if (field.name == "Type")
            {
                if (layout == FieldWidgetLayout::Inspector)
                {
                    static const char* kItems[] = { "Number", "Boolean", "String", "Array", nullptr };

                    BeginInspectorRow(field.name.c_str());
                    if (ImGui::BeginCombo("##type", field.value.c_str()))
                    {
                        for (int i = 0; kItems[i]; ++i)
                        {
                            const bool isSelected = (field.value == kItems[i]);
                            if (ImGui::Selectable(kItems[i], isSelected))
                            {
                                field.value = kItems[i];
                                changed = true;
                            }
                            if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    HandleShortcutToggle("##type");
                }
                else
                {
                    static const char* kTypes[] = {
                        "Number", "Boolean", "String", "Array", nullptr
                    };

                    if (OpPopupCombo("##TypeCombo", field.value, kTypes, 100.0f))
                        changed = true;
                }
            }
            else if (field.name == "Element Type")
            {
                static const char* kItems[] = { "Any", "Number", "Boolean", "String", "Array", nullptr };

                if (layout == FieldWidgetLayout::Inspector)
                {
                    BeginInspectorRow(field.name.c_str());
                    if (ImGui::BeginCombo("##elementType", field.value.c_str()))
                    {
                        for (int i = 0; kItems[i]; ++i)
                        {
                            const bool isSelected = (field.value == kItems[i]);
                            if (ImGui::Selectable(kItems[i], isSelected))
                            {
                                field.value = kItems[i];
                                changed = true;
                            }
                            if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    HandleShortcutToggle("##elementType");
                }
                else
                {
                    if (OpPopupCombo("##ElementTypeCombo", field.value, kItems, 100.0f))
                        changed = true;
                }
            }
            else
            {
                if (layout == FieldWidgetLayout::Inspector)
                {
                    BeginInspectorRow(field.name.c_str());

                    char buf[128] = {};
                    std::strncpy(buf, field.value.c_str(), sizeof(buf) - 1);
                    if (ImGui::InputText("##value", buf, sizeof(buf)))
                    {
                        field.value = buf;
                        changed = true;
                    }
                    HandleShortcutToggle("##value");
                    break;
                }

                ImGui::Text("%s", field.name.c_str());
                ImGui::SameLine();
                ImGui::PushItemWidth(kCompactFieldWidth);

                char buf[128] = {};
                std::strncpy(buf, field.value.c_str(), sizeof(buf) - 1);

                if (ImGui::InputText("##value", buf, sizeof(buf)))
                {
                    field.value = buf;
                    changed = true;
                }
                HandleShortcutToggle("##value");

                ImGui::PopItemWidth();
            }

            break;
        }

        case PinType::Array:
        {
            static const char* kArrayItemTypes[] = {
                "Number", "Boolean", "String", "Array", "Null", nullptr
            };

            std::vector<std::string> items = ParseArrayItems(field.value);
            bool localChanged = false;
            int removeIndex = -1;

            ImGuiID openId = ImGui::GetID("##arrayOpen");
            bool& open = s_arrayOpenState[openId];

            if (ImGui::ArrowButton("##arrayArrow", open ? ImGuiDir_Down : ImGuiDir_Right))
                open = !open;
            ImGui::SameLine();
            ImGui::Text("%s [%d]", field.name.c_str(), static_cast<int>(items.size()));

            if (open)
            {
                ImGui::Indent(12.0f);
                for (int i = 0; i < static_cast<int>(items.size()); ++i)
                {
                    ImGui::PushID(i);
                    ImGui::TextDisabled("%d", i);
                    ImGui::SameLine();

                    ArrayItemType itemType = DetectArrayItemType(items[static_cast<size_t>(i)]);
                    std::string payload = ArrayItemPayload(items[static_cast<size_t>(i)], itemType);

                    std::string selectedType = ArrayItemTypeToLabel(itemType);
                    if (OpPopupCombo("##itemType", selectedType, kArrayItemTypes, 120.0f))
                    {
                        const ArrayItemType parsedType = ArrayItemTypeFromLabel(selectedType);
                        items[static_cast<size_t>(i)] = BuildArrayItemFromPayload(parsedType, payload);
                        localChanged = true;
                    }

                    ImGui::SameLine();

                    itemType = DetectArrayItemType(items[static_cast<size_t>(i)]);
                    payload = ArrayItemPayload(items[static_cast<size_t>(i)], itemType);

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
                        ImGuiID nestedOpenId = ImGui::GetID("##nestedOpen");
                        bool& nestedOpen = s_arrayNestedOpenState[nestedOpenId];
                        const std::vector<std::string> nestedItemsPreview = ParseArrayItems(items[static_cast<size_t>(i)]);

                        if (ImGui::ArrowButton("##nestedArrow", nestedOpen ? ImGuiDir_Down : ImGuiDir_Right))
                        {
                            nestedOpen = !nestedOpen;
                        }
                        ImGui::SameLine();
                        ImGui::TextDisabled("Array [%d]", static_cast<int>(nestedItemsPreview.size()));

                        if (nestedOpen)
                        {
                            std::vector<std::string> nestedItems = nestedItemsPreview;
                            bool nestedChanged = false;
                            int nestedRemove = -1;

                            ImGui::Indent(14.0f);
                            for (int ni = 0; ni < static_cast<int>(nestedItems.size()); ++ni)
                            {
                                ImGui::PushID(ni);
                                ImGui::TextDisabled("%d.%d", i, ni);
                                ImGui::SameLine();

                                ArrayItemType nestedType = DetectArrayItemType(nestedItems[static_cast<size_t>(ni)]);
                                std::string nestedPayload = ArrayItemPayload(nestedItems[static_cast<size_t>(ni)], nestedType);

                                std::string selectedNestedType = ArrayItemTypeToLabel(nestedType);
                                if (OpPopupCombo("##nestedType", selectedNestedType, kArrayItemTypes, 92.0f))
                                {
                                    const ArrayItemType parsedNestedType = ArrayItemTypeFromLabel(selectedNestedType);
                                    nestedItems[static_cast<size_t>(ni)] = BuildArrayItemFromPayload(parsedNestedType, nestedPayload);
                                    nestedChanged = true;
                                }

                                ImGui::SameLine();
                                nestedType = DetectArrayItemType(nestedItems[static_cast<size_t>(ni)]);
                                nestedPayload = ArrayItemPayload(nestedItems[static_cast<size_t>(ni)], nestedType);

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
                                else
                                {
                                    char nbuf[128] = {};
                                    std::strncpy(nbuf, nestedPayload.c_str(), sizeof(nbuf) - 1);

                                    ImGui::PushItemWidth(kCompactFieldWidth);
                                    const bool nestedEdited = ImGui::InputText("##nestedValue", nbuf, sizeof(nbuf));
                                    ImGui::PopItemWidth();

                                    if (nestedEdited)
                                    {
                                        nestedItems[static_cast<size_t>(ni)] = BuildArrayItemFromPayload(nestedType, nbuf);
                                        nestedChanged = true;
                                    }
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
                                const ArrayItemType nextType = nestedItems.empty()
                                    ? ArrayItemType::Null
                                    : DetectArrayItemType(nestedItems.back());
                                nestedItems.push_back(BuildArrayItemFromPayload(nextType, ""));
                                nestedChanged = true;
                            }

                            ImGui::Unindent(14.0f);

                            if (nestedChanged)
                            {
                                items[static_cast<size_t>(i)] = BuildArrayString(nestedItems);
                                localChanged = true;
                            }
                        }
                    }
                    else
                    {
                        char buf[128] = {};
                        std::strncpy(buf, payload.c_str(), sizeof(buf) - 1);

                        ImGui::PushItemWidth(kCompactFieldWidth);
                        const bool itemEdited = ImGui::InputText("##itemValue", buf, sizeof(buf));
                        ImGui::PopItemWidth();

                        if (itemEdited)
                        {
                            items[static_cast<size_t>(i)] = BuildArrayItemFromPayload(itemType, buf);
                            localChanged = true;
                        }
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton("-") && removeIndex < 0)
                        removeIndex = i;

                    ImGui::PopID();
                }

                if (removeIndex >= 0)
                {
                    items.erase(items.begin() + removeIndex);
                    localChanged = true;
                }

                if (ImGui::SmallButton("+ Add element"))
                {
                    const ArrayItemType nextType = items.empty()
                        ? ArrayItemType::Null
                        : DetectArrayItemType(items.back());
                    items.push_back(BuildArrayItemFromPayload(nextType, ""));
                    localChanged = true;
                }

                ImGui::Unindent(12.0f);
            }

            if (localChanged)
            {
                field.value = BuildArrayString(items);
                changed = true;
            }
            break;
        }

        default:
        {
            ImGui::LabelText(field.name.c_str(), "%s", field.value.c_str());
            break;
        }
    }

    ImGui::PopID();
    return changed;
}
