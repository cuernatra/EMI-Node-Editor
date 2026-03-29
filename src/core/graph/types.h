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
