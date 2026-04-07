#pragma once

#include <functional>
#include <string>

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
    NativeCall,  ///< Call a registered native function from flow
    NativeGet,   ///< Read value from a registered native function
    DrawRect,    ///< Draw rectangle preview
    DrawGrid,    ///< Draw grid preview
    DrawCell,    ///< Draw one grid cell (calls drawcell native)
    ClearGrid,   ///< Clear the render grid (calls cleargrid native)
    RenderGrid,  ///< Flush the render grid (calls rendergrid native)
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
    ArrayOperation, ///< Unified array operation node
    Ticker,      ///< Ticker node (phase 2)
    StructDefine, ///< Define struct schema
    StructCreate, ///< Create struct instance
    PreviewPickRect, ///< Preview-picked rectangle position
    While,       ///< While loop
    Variable,    ///< Variable get/set
    CallFunction, ///< Flow call to a user-defined function
    Function,    ///< Function definition node
    Output,      ///< Debug print/output sink
    Unknown      ///< Uninitialized or invalid type
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
