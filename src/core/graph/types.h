#pragma once

#include <functional>

// Shared graph enums (duplicated until call sites are migrated).

enum class PinType
{
    Number,    ///< Numeric values (doubles)
    Boolean,   ///< Boolean true/false
    String,    ///< String values
    Array,     ///< Array/list data structures
    Function,  ///< Function references
    Flow,      ///< Execution flow (control flow)
    Any        ///< Wildcard type - accepts any connection
};

enum class NodeType
{
    Start,       ///< Event start node (flow entry)
    Constant,    ///< Constant value node
    Operator,    ///< Arithmetic/string operators (+, -, *, /)
    Comparison,  ///< Comparison operators (==, !=, <, >)
    Logic,       ///< Logical operators (and, or, not)
    Not,         ///< Boolean negation
    DrawRect,    ///< Preview rectangle drawing node
    DrawGrid,    ///< Preview grid drawing node
    Delay,       ///< Flow delay/order testing node
    Sequence,    ///< Flow sequence splitter (In -> Then 0..N)
    Branch,      ///< Conditional branching (if/else)
    Loop,        ///< Iteration (for/while loops)
    ForEach,     ///< Iterate over array elements
    ArrayGetAt,  ///< Read value from array by index
    ArrayAddAt,  ///< Insert value to array at index
    ArrayReplaceAt, ///< Replace value in array at index
    ArrayRemoveAt, ///< Remove value from array at index
    ArrayLength, ///< Number of items in array
    StructDefine, ///< Defines a named struct schema with custom fields
    StructCreate, ///< Creates one instance of a named struct
    While,       ///< While loop
    Variable,    ///< Variable get/set
    Function,    ///< Function definition/call
    Output,      ///< Graph output/return value
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
