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

const char* PinTypeToString(PinType t)
{
    switch (t)
    {
        case PinType::Number:   return "Number";
        case PinType::Boolean:  return "Boolean";
        case PinType::String:   return "String";
        case PinType::Array:    return "Array";
        case PinType::Function: return "Function";
        case PinType::Flow:     return "Flow";
        default:                return "Any";
    }
}

PinType PinTypeFromString(std::string_view s)
{
    if (s == "Number")   return PinType::Number;
    if (s == "Boolean")  return PinType::Boolean;
    if (s == "String")   return PinType::String;
    if (s == "Array")    return PinType::Array;
    if (s == "Function") return PinType::Function;
    if (s == "Flow")     return PinType::Flow;
    return PinType::Any;
}

bool IsValuePinType(PinType t)
{
    switch (t)
    {
        case PinType::Any:
        case PinType::Number:
        case PinType::Boolean:
        case PinType::String:
        case PinType::Array:
            return true;
        default:
            return false;
    }
}

PinType ValuePinTypeFromString(std::string_view s, PinType fallback)
{
    const PinType parsed = PinTypeFromString(s);
    return IsValuePinType(parsed) ? parsed : fallback;
}

const char* ValuePinTypeToString(PinType t)
{
    return IsValuePinType(t) ? PinTypeToString(t) : "Number";
}
