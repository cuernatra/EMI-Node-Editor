/** @file pin.h */
/** @brief Pin types, styles, and helpers for the graph editor. */

#ifndef PIN_H
#define PIN_H

#include "imgui_node_editor.h"
#include "imgui.h"
#include "types.h"
#include <string>
#include <functional>

namespace ed = ax::NodeEditor;



/** @brief Connection point on a node. */
struct Pin
{
    ed::PinId   id{};                                      ///< Unique ID (imgui-node-editor handle)
    ed::NodeId  parentNodeId{};                            ///< ID of the owning VisualNode
    NodeType    parentNodeType = NodeType::Unknown;       ///< Type of the owning node
    std::string name;                                     ///< Display label
    PinType     type       = PinType::Any;                ///< Data type this pin accepts/produces
    bool        isInput    = true;                        ///< true = input pin, false = output pin
    bool        isMultiInput = false;                     ///< Allow multiple incoming connections

    /** @brief Returns the display color for this pin type. */
    ImVec4 GetTypeColor() const;

    /** @brief Returns true if output can connect to input. */
    static bool CanConnect(const Pin& output, const Pin& input);
};

/** @brief Color palette for pin types. */
struct PinStylePalette
{
    ImVec4 number   = {1.0f, 0.8f, 0.0f, 1.0f};
    ImVec4 boolean  = {1.0f, 0.0f, 0.0f, 1.0f};
    ImVec4 string   = {0.0f, 1.0f, 0.0f, 1.0f};
    ImVec4 array    = {0.0f, 0.0f, 1.0f, 1.0f};
    ImVec4 function = {1.0f, 0.0f, 1.0f, 1.0f};
    ImVec4 flow     = {1.0f, 1.0f, 1.0f, 1.0f};
    ImVec4 any      = {0.5f, 0.5f, 0.5f, 1.0f};
};

enum class PinIconType
{
    Circle,
    Flow
};

/** @brief Icon mapping for pin types. */
struct PinIconPalette
{
    PinIconType number   = PinIconType::Circle;
    PinIconType boolean  = PinIconType::Circle;
    PinIconType string   = PinIconType::Circle;
    PinIconType array    = PinIconType::Circle;
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

/** @brief Creates and returns a pin instance. */
Pin MakePin(uint32_t id, ed::NodeId parentNodeId, NodeType parentNodeType,
            std::string name, PinType type, bool isInput,
            bool isMultiInput = false);

#endif // PIN_H
