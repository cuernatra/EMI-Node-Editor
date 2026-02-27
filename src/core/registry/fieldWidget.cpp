#include "fieldWidget.h"
#include "imgui.h"
#include "imgui_node_editor.h"
#include <cstdlib>
#include <cstring>
#include <array>
#include <unordered_map>

namespace ed = ax::NodeEditor;

// HELPERS

static float ParseFloat(const std::string& s)
{
    return s.empty() ? 0.0f : std::stof(s);
}

static bool ParseBool(const std::string& s)
{
    return s == "1" || s == "true" || s == "True";
}

// Track previous frame's focus state for EACH field. Needed to detect when focus changes
// (gained or lost), not just whether it's focused now. Allows us to disable shortcuts
// exactly when user starts typing in a field, and re-enable them when done.
static std::unordered_map<ImGuiID, bool> s_wasActive;

// Handle node editor shortcut toggling when input widgets gain/lose focus
static void HandleShortcutToggle(const std::string& fieldName)
{
    ImGuiID id = ImGui::GetID(fieldName.c_str());
    bool isActive = ImGui::IsItemActive();
    bool wasActive = s_wasActive[id];

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

bool DrawField(NodeField& field)
{
    bool changed = false;

    // Give each field a stable ImGui ID scope using its name.
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
            HandleShortcutToggle(field.name);
            
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
            break;
        }
        case PinType::String:
        {
            // Special-case known operator fields to render as combos.
            if (field.name == "Op")
            {
                // Three dropdown lists, one for each operator category.
                // The descriptor sets a default, but we detect which category it belongs to
                // so we show the RIGHT dropdown. (User shouldn't see arithmetic ops in a Logic node.)
                static const char* kArith[] = { "+", "-", "*", "/", nullptr };
                static const char* kCmp[]   = { "==", "!=", "<", "<=", ">", ">=", nullptr };
                static const char* kLogic[] = { "AND", "OR", "NOT", nullptr };

                // Auto-detect which dropdown to show based on current field value.
                // This handles the case where a node was loaded from disk with a value
                // we need to categorize, OR where the default was set.
                const char** items = kArith;  // Default to arithmetic
                if (field.value == "==" || field.value == "!=" ||
                    field.value == "<"  || field.value == "<=" ||
                    field.value == ">"  || field.value == ">=")
                    items = kCmp;  // Value is a comparison operator
                else if (field.value == "AND" || field.value == "OR" ||
                         field.value == "NOT")
                    items = kLogic;  // Value is a logic operator

                // Find which index in the current dropdown list matches the current value.
                // This ensures the combo shows the correct item as selected.
                int current = 0;
                for (int i = 0; items[i] != nullptr; ++i)
                    if (field.value == items[i]) { current = i; break; }

                // Render the ImGui combo dropdown. When user selects, 'current' updates.
                if (ImGui::Combo(field.name.c_str(), &current, items,
                                 /* count */ [](const char** arr) {
                                     int n = 0; while (arr[n]) ++n; return n;
                                 }(items)))
                {
                    field.value = items[current];  // Store user's selection
                    changed = true;
                }
            }
            else
            {
                // Generic text input.
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
                HandleShortcutToggle(field.name);
                
                ImGui::PopItemWidth();
            }
            break;
        }

        case PinType::Array:
        {
            // TODO: Show item count as read-only for now; editing arrays is a
            // larger problem that deserves its own widget later.
            ImGui::LabelText(field.name.c_str(), "%s", field.value.c_str());
            break;
        }

        default:
        {
            // Fallback: plain text display.
            ImGui::LabelText(field.name.c_str(), "%s", field.value.c_str());
            break;
        }
    }

    ImGui::PopID();
    return changed;
}
