/**
 * @file pin.h
 * @brief Pin definitions for the visual graph
 * 
 * Defines the data structures for pins (node connection points) and
 * helper functions for type compatibility and pin creation.
 * 
 */

#ifndef PIN_H
#define PIN_H

#include "imgui_node_editor.h"
#include "imgui.h"
#include "types.h"
#include <string>
#include <functional>

namespace ed = ax::NodeEditor;



/**
 * @brief Connection point on a node
 * 
 * Represents an input or output pin on a visual node. Pins have a data type,
 * direction (input/output), and can be connected via links. The visual
 * appearance (color) is determined by the pin's type.
 */
struct Pin
{
    ed::PinId   id{};                                      ///< Unique ID (imgui-node-editor handle)
    ed::NodeId  parentNodeId{};                            ///< ID of the owning VisualNode
    NodeType    parentNodeType = NodeType::Unknown;       ///< Type of the owning node
    std::string name;                                     ///< Display label
    PinType     type       = PinType::Any;                ///< Data type this pin accepts/produces
    bool        isInput    = true;                        ///< true = input pin, false = output pin
    bool        isMultiInput = false;                     ///< Allow multiple incoming connections

    // ------------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------------

    /**
     * @brief Get the color for this pin based on its type
     * @return RGBA color vector for rendering
     */
    ImVec4 GetTypeColor() const
    {
        switch (type)
        {
            case PinType::Number:   return {1.0f, 0.8f, 0.0f, 1.0f}; // Yellow
            case PinType::Boolean:  return {1.0f, 0.0f, 0.0f, 1.0f}; // Red
            case PinType::String:   return {0.0f, 1.0f, 0.0f, 1.0f}; // Green
            case PinType::Array:    return {0.0f, 0.0f, 1.0f, 1.0f}; // Blue
            case PinType::Function: return {1.0f, 0.0f, 1.0f, 1.0f}; // Magenta
            case PinType::Flow:     return {1.0f, 1.0f, 1.0f, 1.0f}; // White
            case PinType::Any:
            default:                return {0.5f, 0.5f, 0.5f, 1.0f}; // Gray
        }
    }

    /**
     * @brief Check if two pins can be connected
     * @param output The source (output) pin
     * @param input The destination (input) pin
     * @return true if a valid connection can be made
     * 
     * Validates:
     * - Direction: output must be output pin, input must be input pin
     * - Type: Types must match, or one side must be PinType::Any
     */
    static bool CanConnect(const Pin& output, const Pin& input)
    {
        if (output.isInput || !input.isInput)
            return false; // direction mismatch

        if (output.type == PinType::Any || input.type == PinType::Any)
            return true;

        return output.type == input.type;
    }
};

// ---------------------------------------------------------------------------
// Factory helpers
// ---------------------------------------------------------------------------

/**
 * @brief Create a fully initialized pin
 * @param id Unique numeric ID
 * @param parentNodeId ID of the node that owns this pin
 * @param parentNodeType Type of the owning node
 * @param name Display label for the pin
 * @param type Data type this pin handles
 * @param isInput true for input pin, false for output pin
 * @param isMultiInput Whether this input accepts multiple connections
 * @return Fully initialized Pin struct
 */
inline Pin MakePin(uint32_t id, ed::NodeId parentNodeId, NodeType parentNodeType,
                   std::string name, PinType type, bool isInput,
                   bool isMultiInput = false)
{
    Pin p;
    p.id = ed::PinId(id);
    p.parentNodeId   = parentNodeId;
    p.parentNodeType = parentNodeType;
    p.name           = std::move(name);
    p.type           = type;
    p.isInput        = isInput;
    p.isMultiInput   = isMultiInput;
    return p;
}

// ---------------------------------------------------------------------------
// NodeType conversion helpers
// ---------------------------------------------------------------------------

#endif // PIN_H
