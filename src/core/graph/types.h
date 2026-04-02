#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <cctype>

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
    GridNodeSchema, ///< Legacy/removed
    GridNodeCreate, ///< Legacy/removed
    GridNodeUpdate, ///< Legacy/removed
    GridNodeDelete, ///< Legacy/removed
    StructDefine, ///< Defines a named struct schema with custom fields
    StructCreate, ///< Creates one instance of a named struct
    StructGetField, ///< Legacy/removed
    StructSetField, ///< Legacy/removed
    StructDelete, ///< Legacy/removed
    While,       ///< While loop
    Variable,    ///< Variable get/set
    Function,    ///< Function definition/call
    Output,      ///< Graph output/return value
    Unknown      ///< Uninitialized or invalid type
};

inline const char* NodeTypeToString(NodeType t)
{
    switch (t)
    {
        case NodeType::Start:      return "Start";
        case NodeType::Constant:   return "Constant";
        case NodeType::Operator:   return "Operator";
        case NodeType::Comparison: return "Comparison";
        case NodeType::Logic:      return "Logic";
        case NodeType::Not:        return "Not";
        case NodeType::DrawRect:   return "Draw Rect";
        case NodeType::DrawGrid:   return "Draw Grid";
        case NodeType::Delay:      return "Delay";
        case NodeType::Sequence:   return "Sequence";
        case NodeType::Branch:     return "Branch";
        case NodeType::Loop:       return "Loop";
        case NodeType::ForEach:    return "For Each";
        case NodeType::ArrayGetAt: return "Array Get";
        case NodeType::ArrayAddAt: return "Array Add";
        case NodeType::ArrayReplaceAt: return "Array Replace";
        case NodeType::ArrayRemoveAt: return "Array Remove";
        case NodeType::ArrayLength: return "Array Length";
        case NodeType::GridNodeSchema: return "Grid Node Schema";
        case NodeType::GridNodeCreate: return "Grid Node Create";
        case NodeType::GridNodeUpdate: return "Grid Node Update";
        case NodeType::GridNodeDelete: return "Grid Node Delete";
        case NodeType::StructDefine: return "Struct Define";
        case NodeType::StructCreate: return "Struct Create";
        case NodeType::StructGetField: return "Struct Get Field";
        case NodeType::StructSetField: return "Struct Set Field";
        case NodeType::StructDelete: return "Struct Delete";
        case NodeType::While:      return "While";
        case NodeType::Variable:   return "Variable";
        case NodeType::Function:   return "Function";
        case NodeType::Output:     return "Debug Print";
        default:                   return "Unknown";
    }
}

inline std::string RemoveWhitespace(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char ch : s)
    {
        if (!std::isspace(ch))
            out.push_back(static_cast<char>(ch));
    }
    return out;
}

// Stable, single-token identifier for serialization.
// This must NOT contain whitespace because the graph file format uses whitespace tokenization.
// Derived from the UI label to avoid per-node manual token maintenance.
inline std::string NodeTypeToSaveToken(NodeType t)
{
    return RemoveWhitespace(NodeTypeToString(t));
}

inline NodeType NodeTypeFromString(const std::string& s)
{
    const std::string norm = RemoveWhitespace(s);

    if (norm == "Start")      return NodeType::Start;
    if (norm == "Constant")   return NodeType::Constant;
    if (norm == "Operator")   return NodeType::Operator;
    if (norm == "Comparison") return NodeType::Comparison;
    if (norm == "Logic")      return NodeType::Logic;
    if (norm == "Not")        return NodeType::Not;
    if (norm == "DrawRect")   return NodeType::DrawRect;
    if (norm == "DrawGrid")   return NodeType::DrawGrid;
    if (norm == "Delay")      return NodeType::Delay;
    if (norm == "Sequence")   return NodeType::Sequence;
    if (norm == "Branch")     return NodeType::Branch;
    if (norm == "Loop")       return NodeType::Loop;

    if (norm == "ForEach")       return NodeType::ForEach;
    if (norm == "ArrayGet")      return NodeType::ArrayGetAt;
    if (norm == "ArrayAdd")      return NodeType::ArrayAddAt;
    if (norm == "ArrayReplace")  return NodeType::ArrayReplaceAt;
    if (norm == "ArrayRemove")   return NodeType::ArrayRemoveAt;
    if (norm == "ArrayLength")   return NodeType::ArrayLength;
    if (norm == "ArrayCount")    return NodeType::ArrayLength;

    if (norm == "GridNodeSchema") return NodeType::Unknown;
    if (norm == "GridNodeCreate") return NodeType::Unknown;
    if (norm == "GridNodeUpdate") return NodeType::Unknown;
    if (norm == "GridNodeDelete") return NodeType::Unknown;

    if (norm == "StructDefine")   return NodeType::StructDefine;
    if (norm == "StructCreate")   return NodeType::StructCreate;
    if (norm == "StructGetField") return NodeType::Unknown;
    if (norm == "StructSetField") return NodeType::Unknown;
    if (norm == "StructDelete")   return NodeType::Unknown;

    if (norm == "While")        return NodeType::While;
    if (norm == "Variable")     return NodeType::Variable;
    if (norm == "Function")     return NodeType::Function;
    if (norm == "DebugPrint")   return NodeType::Output;
    return NodeType::Unknown;
}

// TODO: mitä helee??
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
