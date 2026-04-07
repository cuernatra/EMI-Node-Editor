/** @file pin.h */
/** @brief Pin data and helper functions for graph connections. */

#ifndef PIN_H
#define PIN_H

#include "imgui_node_editor.h"
#include "types.h"
#include <string>
#include <string_view>
 

namespace ed = ax::NodeEditor;



/** @brief One input or output connection point on a node. */
struct Pin
{
    ed::PinId   id{};                                      ///< Unique pin id.
    ed::NodeId  parentNodeId{};                            ///< Owning node id.
    NodeType    parentNodeType = NodeType::Unknown;       ///< Owning node type.
    std::string name;                                     ///< Label shown in UI.
    PinType     type       = PinType::Any;                ///< Data type accepted/produced.
    bool        isInput    = true;                        ///< true=input, false=output.
    bool        isMultiInput = false;                     ///< Allow multiple incoming wires.

    /** @brief Returns true when an output pin can connect to an input pin. */
    static bool CanConnect(const Pin& output, const Pin& input);
};

/** @brief Create a pin struct with the given values. */
Pin MakePin(uint32_t id, ed::NodeId parentNodeId, NodeType parentNodeType,
            std::string name, PinType type, bool isInput,
            bool isMultiInput = false);

/** @brief Convert pin type to save/log string token. */
const char* PinTypeToString(PinType t);

/** @brief Parse pin type token (unknown -> Any). */
PinType PinTypeFromString(std::string_view s);

/**
 * @brief True for value-carrying pin types (not Flow/Function).
 *
 * Value types are those that can be stored in NodeField and edited in the inspector.
 */
bool IsValuePinType(PinType t);

/**
 * @brief Parse a value-type token.
 *
 * Unlike PinTypeFromString, this never returns Flow/Function.
 * Unknown/non-value tokens return fallback (default Number).
 */
PinType ValuePinTypeFromString(std::string_view s, PinType fallback = PinType::Number);

/**
 * @brief Convert value pin type to stable string token.
 *
 * Non-value types return "Number".
 */
const char* ValuePinTypeToString(PinType t);

#endif // PIN_H
