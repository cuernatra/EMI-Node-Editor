/** @file pin.h */
/** @brief Pin types, styles, and helpers for the graph editor. */

#ifndef PIN_H
#define PIN_H

#include "imgui_node_editor.h"
#include "types.h"
#include <string>
#include <string_view>
 

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

    /** @brief Returns true if output can connect to input. */
    static bool CanConnect(const Pin& output, const Pin& input);
};

/** @brief Creates and returns a pin instance. */
Pin MakePin(uint32_t id, ed::NodeId parentNodeId, NodeType parentNodeType,
            std::string name, PinType type, bool isInput,
            bool isMultiInput = false);

/** @brief Stable, single-token string for serialization/logging. */
const char* PinTypeToString(PinType t);

/** @brief Parse PinType from a string token (unknown -> Any). */
PinType PinTypeFromString(std::string_view s);

#endif // PIN_H
