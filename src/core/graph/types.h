#pragma once

#include <functional>

// Shared graph enums.

enum class PinType
{
    Number,    ///< Number value
    Boolean,   ///< true/false value
    String,    ///< Text value
    Array,     ///< List/array value
    Function,  ///< Function reference
    Flow,      ///< Execution flow signal
    Any        ///< Wildcard type
};

enum class NodeType
{
    Start,       ///< Graph entry node
    Constant,    ///< Constant value node
    Operator,    ///< Math/string operation
    Comparison,  ///< Comparison operation
    Logic,       ///< Logic operation
    Not,         ///< Boolean negate
    DrawRect,    ///< Draw rectangle preview
    DrawGrid,    ///< Draw grid preview
    Delay,       ///< Delay in flow
    Sequence,    ///< One input, many ordered outputs
    Branch,      ///< If/else branching
    Loop,        ///< Counted loop
    ForEach,     ///< Loop through array items
    ArrayGetAt,  ///< Read array value at index
    ArrayAddAt,  ///< Insert array value at index
    ArrayReplaceAt, ///< Replace array value at index
    ArrayRemoveAt, ///< Remove array value at index
    ArrayLength, ///< Array item count
    StructDefine, ///< Define struct schema
    StructCreate, ///< Create struct instance
    While,       ///< While loop
    Variable,    ///< Variable get/set
    Function,    ///< Function define/call
    Output,      ///< Graph return/output
    Unknown      ///< Unknown type
};

namespace std {
    template<>
    struct hash<NodeType>
    {
        std::size_t operator()(NodeType t) const noexcept
        {
            return std::hash<int>()(static_cast<int>(t));
        }
    };
}
