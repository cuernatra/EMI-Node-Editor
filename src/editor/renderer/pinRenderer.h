#pragma once

#include "imgui.h"
#include "imgui_node_editor.h"

#include "types.h"

namespace ed = ax::NodeEditor;

/** @brief Color palette for pin types. */
struct PinStylePalette
{
    ImVec4 number   = {1.0f, 0.8f, 0.0f, 1.0f};
    ImVec4 boolean  = {1.0f, 0.0f, 0.0f, 1.0f};
    ImVec4 string   = {0.0f, 1.0f, 0.0f, 1.0f};
    ImVec4 array    = {0.0f, 0.0f, 1.0f, 1.0f};
    ImVec4 struct_  = {1.0f, 0.5f, 0.0f, 1.0f};   ///< Orange — distinct from Array (blue)
    ImVec4 function = {1.0f, 0.0f, 1.0f, 1.0f};
    ImVec4 flow     = {1.0f, 1.0f, 1.0f, 1.0f};
    ImVec4 any      = {0.5f, 0.5f, 0.5f, 1.0f};
};

enum class PinIconType
{
    Circle,
    Diamond,
    Flow
};

/** @brief Icon mapping for pin types. */
struct PinIconPalette
{
    PinIconType number   = PinIconType::Circle;
    PinIconType boolean  = PinIconType::Circle;
    PinIconType string   = PinIconType::Circle;
    PinIconType array    = PinIconType::Circle;
    PinIconType struct_  = PinIconType::Diamond;
    PinIconType function = PinIconType::Circle;
    PinIconType flow     = PinIconType::Flow;
    PinIconType any      = PinIconType::Circle;
    bool filled          = true;
};

/** @brief Node-editor pin style settings. */
struct EditorPinStyle
{
    float pinRounding    = 4.0f;
    float pinBorderWidth = 0.0f;
    float pinRadius      = 0.0f;
    float pinArrowSize   = 0.0f;
    float pinArrowWidth  = 0.0f;
};

const PinStylePalette& GetPinStylePalette();
void SetPinStylePalette(const PinStylePalette& palette);
ImVec4 GetPinTypeColor(PinType type);

const PinIconPalette& GetPinIconPalette();
void SetPinIconPalette(const PinIconPalette& palette);
PinIconType GetPinTypeIcon(PinType type);
void DrawPinIcon(ImDrawList* drawList,
                 const ImVec2& a,
                 const ImVec2& b,
                 PinIconType type,
                 bool filled,
                 ImU32 color,
                 ImU32 innerColor);

const EditorPinStyle& GetEditorPinStyle();
void SetEditorPinStyle(const EditorPinStyle& style);
void ApplyEditorPinStyle(ed::Style& style);
