#include "fieldWidget.h"
#include "imgui.h"
#include "imgui_node_editor.h"
#include <cstdlib>
#include <cstring>
#include <array>
#include <unordered_map>

namespace ed = ax::NodeEditor;

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

static std::unordered_map<ImGuiID, bool> s_wasActive;

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
        ImGui::GetColorU32(ImGuiCol_Text)
    );

    ImGui::PopStyleVar();

    if (open)
        ImGui::OpenPopup("##popup");

    ImVec2 desiredPos(leftMin.x, rightMax.y);

    int itemCount = 0;
    while (items[itemCount]) ++itemCount;

    ImGuiStyle& style = ImGui::GetStyle();
    float lineH  = ImGui::GetTextLineHeightWithSpacing();
    float popupH = itemCount * lineH + style.WindowPadding.y * 2.0f;

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
    bool changed = false;

    ImGui::PushID(field.name.c_str());

    switch (field.valueType)
    {
        case PinType::Number:
        {
            ImGui::Text("%s", field.name.c_str());
            ImGui::SameLine();
            ImGui::PushItemWidth(100.0f);

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
            else if (field.name == "Type")
            {
                static const char* kTypes[] = {
                    "Number", "Boolean", "String", "Array", nullptr
                };

                if (OpPopupCombo("##TypeCombo", field.value, kTypes, 100.0f))
                    changed = true;
            }
            else
            {
                ImGui::Text("%s", field.name.c_str());
                ImGui::SameLine();
                ImGui::PushItemWidth(100.0f);

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
            ImGui::Text("%s", field.name.c_str());
            ImGui::SameLine();
            ImGui::PushItemWidth(100.0f);

            char buf[128] = {};
            std::strncpy(buf, field.value.c_str(), sizeof(buf) - 1);

            if (ImGui::InputText("##value", buf, sizeof(buf)))
            {
                field.value = buf;
                changed = true;
            }
            HandleShortcutToggle("##value");

            ImGui::PopItemWidth();
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