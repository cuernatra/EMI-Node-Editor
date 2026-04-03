#pragma once

#include "core/graph/types.h"

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

inline NodeColorCategory GetNodeColorCategory(NodeType type)
{
    switch (type)
    {
        case NodeType::Start:
            return NodeColorCategory::Event;

        case NodeType::Constant:
        case NodeType::Variable:
        case NodeType::ArrayGetAt:
        case NodeType::ArrayLength:
            return NodeColorCategory::Data;

        case NodeType::StructDefine:
        case NodeType::StructCreate:
        case NodeType::StructGetField:
        case NodeType::StructSetField:
        case NodeType::StructDelete:
            return NodeColorCategory::Struct;

        case NodeType::Operator:
        case NodeType::Comparison:
        case NodeType::Logic:
        case NodeType::Not:
        case NodeType::DrawGrid:
        case NodeType::Function:
            return NodeColorCategory::Logic;

        case NodeType::Delay:
        case NodeType::DrawRect:
        case NodeType::Sequence:
        case NodeType::Branch:
        case NodeType::Loop:
        case NodeType::ForEach:
        case NodeType::ArrayAddAt:
        case NodeType::ArrayReplaceAt:
        case NodeType::ArrayRemoveAt:
        case NodeType::While:
        case NodeType::Output:
            return NodeColorCategory::Flow;

        default:
            return NodeColorCategory::More;
    }
}
