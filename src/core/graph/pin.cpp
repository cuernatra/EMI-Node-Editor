/**
 * @file pin.cpp
 * @brief Pin implementations for the visual graph
 */

#include "pin.h"

#include <utility>

bool Pin::CanConnect(const Pin& output, const Pin& input)
{
    if (output.isInput || !input.isInput)
        return false; // direction mismatch

    if (output.type == PinType::Any || input.type == PinType::Any)
        return true;

    return output.type == input.type;
}

Pin MakePin(uint32_t id, ed::NodeId parentNodeId, NodeType parentNodeType,
            std::string name, PinType type, bool isInput, bool isMultiInput)
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
