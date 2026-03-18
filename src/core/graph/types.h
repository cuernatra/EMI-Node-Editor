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
    Constant,    ///< Constant value node
    Operator,    ///< Arithmetic/string operators (+, -, *, /)
    Comparison,  ///< Comparison operators (==, !=, <, >)
    Logic,       ///< Logical operators (and, or, not)
    Sequence,    ///< Flow sequence splitter (In -> Then 0..N)
    Branch,      ///< Conditional branching (if/else)
    Loop,        ///< Iteration (for/while loops)
    Start,       ///< Flow entry point (explicit graph start)
    Variable,    ///< Variable get/set
    Function,    ///< Function definition/call
    FunctionCall,///< Function call node (variadic args)
    Output,      ///< Graph output/return value
    Unknown      ///< Uninitialized or invalid type
};

inline const char* NodeTypeToString(NodeType t)
{
    switch (t)
    {
        case NodeType::Constant:   return "Constant";
        case NodeType::Operator:   return "Operator";
        case NodeType::Comparison: return "Comparison";
        case NodeType::Logic:      return "Logic";
        case NodeType::Sequence:   return "Sequence";
        case NodeType::Branch:     return "Branch";
        case NodeType::Loop:       return "Loop";
        case NodeType::Start:      return "Start";
        case NodeType::Variable:   return "Variable";
        case NodeType::Function:   return "Function";
        case NodeType::FunctionCall:return "FunctionCall";
        case NodeType::Output:     return "Output";
        default:                   return "Unknown";
    }
}

inline NodeType NodeTypeFromString(const std::string& s)
{
    if (s == "Constant")   return NodeType::Constant;
    if (s == "Operator")   return NodeType::Operator;
    if (s == "Comparison") return NodeType::Comparison;
    if (s == "Logic")      return NodeType::Logic;
    if (s == "Sequence")   return NodeType::Sequence;
    if (s == "Branch")     return NodeType::Branch;
    if (s == "Loop")       return NodeType::Loop;
    if (s == "Start")      return NodeType::Start;
    if (s == "Variable")   return NodeType::Variable;
    if (s == "Function")   return NodeType::Function;
    if (s == "FunctionCall") return NodeType::FunctionCall;
    if (s == "Output")     return NodeType::Output;
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
