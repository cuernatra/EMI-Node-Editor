#pragma once

#include <string>
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
    ArrayRemoveAt, ///< Remove value from array at index
    GridNodeSchema, ///< Defines default grid-node struct layout/data
    GridNodeCreate, ///< Creates one grid-node instance (struct-as-array)
    GridNodeUpdate, ///< Returns updated grid-node instance
    GridNodeDelete, ///< Removes grid-node instance from array by index
    StructDefine, ///< Defines a named struct schema with custom fields
    StructCreate, ///< Creates one instance of a named struct
    StructGetField, ///< Reads one field value from a named struct instance
    StructSetField, ///< Writes one field value and returns updated struct instance
    StructDelete, ///< Removes one struct instance from array by Id input
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
        case NodeType::ArrayRemoveAt: return "Array Remove";
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

inline NodeType NodeTypeFromString(const std::string& s)
{
    if (s == "Start")      return NodeType::Start;
    if (s == "Constant")   return NodeType::Constant;
    if (s == "Operator")   return NodeType::Operator;
    if (s == "Comparison") return NodeType::Comparison;
    if (s == "Logic")      return NodeType::Logic;
    if (s == "Not")        return NodeType::Not;
    if (s == "DrawRect")   return NodeType::DrawRect;
    if (s == "Draw Rect")  return NodeType::DrawRect;
    if (s == "DrawGrid")   return NodeType::DrawGrid;
    if (s == "Draw Grid")  return NodeType::DrawGrid;
    if (s == "Delay")      return NodeType::Delay;
    if (s == "Sequence")   return NodeType::Sequence;
    if (s == "Branch")     return NodeType::Branch;
    if (s == "Loop")       return NodeType::Loop;
    if (s == "ForEach")    return NodeType::ForEach;
    if (s == "For Each")   return NodeType::ForEach;
    if (s == "ArrayGetAt") return NodeType::ArrayGetAt;
    if (s == "Array Get")  return NodeType::ArrayGetAt;
    if (s == "ArrayAddAt") return NodeType::ArrayAddAt;
    if (s == "Array Add")  return NodeType::ArrayAddAt;
    if (s == "ArrayRemoveAt") return NodeType::ArrayRemoveAt;
    if (s == "Array Remove")  return NodeType::ArrayRemoveAt;
    if (s == "GridNodeSchema") return NodeType::GridNodeSchema;
    if (s == "Grid Node Schema") return NodeType::GridNodeSchema;
    if (s == "GridNodeCreate") return NodeType::GridNodeCreate;
    if (s == "Grid Node Create") return NodeType::GridNodeCreate;
    if (s == "GridNodeUpdate") return NodeType::GridNodeUpdate;
    if (s == "Grid Node Update") return NodeType::GridNodeUpdate;
    if (s == "GridNodeDelete") return NodeType::GridNodeDelete;
    if (s == "Grid Node Delete") return NodeType::GridNodeDelete;
    if (s == "StructDefine") return NodeType::StructDefine;
    if (s == "Struct Define") return NodeType::StructDefine;
    if (s == "StructCreate") return NodeType::StructCreate;
    if (s == "Struct Create") return NodeType::StructCreate;
    if (s == "StructGetField") return NodeType::StructGetField;
    if (s == "Struct Get Field") return NodeType::StructGetField;
    if (s == "StructSetField") return NodeType::StructSetField;
    if (s == "Struct Set Field") return NodeType::StructSetField;
    if (s == "StructDelete") return NodeType::StructDelete;
    if (s == "Struct Delete") return NodeType::StructDelete;
    if (s == "While")      return NodeType::While;
    if (s == "Variable")   return NodeType::Variable;
    if (s == "Function")   return NodeType::Function;
    if (s == "Output")      return NodeType::Output;      // backward compatibility
    if (s == "Debug Print") return NodeType::Output;
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
