#pragma once

#include "core/registry/nodeDescriptor.h"

enum class NodeColorCategory
{
    Event,
    Data,
    Struct,
    Logic,
    Flow,
    More
};

inline const char* NodeColorCategoryLabel(NodeColorCategory category)
{
    switch (category)
    {
        case NodeColorCategory::Event:  return "Events";
        case NodeColorCategory::Data:   return "Data";
        case NodeColorCategory::Struct: return "Structs";
        case NodeColorCategory::Logic:  return "Logic";
        case NodeColorCategory::Flow:   return "Flow";
        case NodeColorCategory::More:   return "More";
        default:                        return "More";
    }
}

inline NodeColorCategory GetNodeColorCategory(const std::string& category)
{
    if (category == "Events")
        return NodeColorCategory::Event;

    if (category == "Data")
        return NodeColorCategory::Data;

    if (category == "Structs")
        return NodeColorCategory::Struct;

    if (category == "Logic")
        return NodeColorCategory::Logic;

    if (category == "Flow")
        return NodeColorCategory::Flow;

    return NodeColorCategory::More;
}

inline NodeColorCategory GetNodeColorCategory(const NodeDescriptor* descriptor)
{
    if (!descriptor)
        return NodeColorCategory::More;

    return GetNodeColorCategory(descriptor->category);
}

inline NodeColorCategory GetNodeColorCategory(const char* category)
{
    return category ? GetNodeColorCategory(std::string(category)) : NodeColorCategory::More;
}
