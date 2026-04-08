#pragma once

#include "graphCompiler.h"
#include "astBuilders.h"
#include "textParseHelpers.h"
#include "../graph/visualNodeUtils.h"
#include "../registry/nodeDescriptor.h"
#include "../graph/pin.h"
#include "../graph/nodeField.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

/**
 * Helper functions for node compile callbacks.
 * Include this from src/core/registry/nodes/ — or use the local re-export there.
 * Functions return Node* — if you return early on error, delete any nodes
 * you created that haven't been attached to a parent yet.
 */

namespace
{

using emi::ast::MakeNode;
using emi::ast::MakeIdNode;
using emi::ast::MakeBinaryOpNode;
using emi::ast::MakeUnaryOpNode;
using emi::ast::MakeFunctionCallNode;
using emi::ast::MakeIndexerNode;
using emi::ast::MakeNumberLiteral;
using emi::ast::MakeBoolLiteral;
using emi::ast::MakeStringLiteral;

/**
 * Load a node's pins and fields from a saved descriptor during graph load.
 * Returns false if the saved pin count doesn't match the descriptor (file is outdated or corrupt).
 *
 * @code
 * if (!PopulateExactPinsAndFields(node, *desc, savedPinIds))
 *     return false;
 * @endcode
 */
inline bool PopulateExactPinsAndFields(VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds)
{
    if (pinIds.size() != desc.pins.size())
        return false;

    size_t pinIndex = 0;
    for (const PinDescriptor& pd : desc.pins)
    {
        const uint32_t pinId = static_cast<uint32_t>(pinIds[pinIndex++]);
        Pin p = MakePin(pinId, n.id, desc.type, pd.name, pd.type, pd.isInput, pd.isMultiInput);
        if (pd.isInput)
            n.inPins.push_back(p);
        else
            n.outPins.push_back(p);
    }

    for (const FieldDescriptor& fd : desc.fields)
        n.fields.push_back(MakeNodeField(fd));

    return true;
}

/** Map operator UI string ("+", "-", "*", "/") to a Token. Returns Token::None if unknown. */
inline Token ParseOperatorToken(const std::string* opStr)
{
    if (!opStr || *opStr == "+") return Token::Add;
    if (*opStr == "-") return Token::Sub;
    if (*opStr == "*") return Token::Mult;
    if (*opStr == "/") return Token::Div;
    return Token::None;
}

/** Map comparison UI string ("==", "!=", "<", "<=", ">", ">=") to a Token. Returns Token::None if unknown. */
inline Token ParseComparisonToken(const std::string* opStr)
{
    if (!opStr || *opStr == "==") return Token::Equal;
    if (*opStr == "!=") return Token::NotEqual;
    if (*opStr == "<") return Token::Less;
    if (*opStr == "<=") return Token::LessEqual;
    if (*opStr == ">") return Token::Larger;
    if (*opStr == ">=") return Token::LargerEqual;
    return Token::None;
}

/** Map logic UI string ("AND", "OR") to a Token. Returns Token::None if unknown. */
inline Token ParseLogicToken(const std::string* opStr)
{
    if (!opStr || *opStr == "AND") return Token::And;
    if (*opStr == "OR") return Token::Or;
    return Token::None;
}

/** Map a variable Type field string ("Number", "Boolean", "String", "Array") to a PinType. */
inline PinType VariableTypeFromString(const std::string& typeName)
{
    if (typeName == "Boolean") return PinType::Boolean;
    if (typeName == "String")  return PinType::String;
    if (typeName == "Array")   return PinType::Array;
    return PinType::Number;
}

/** Build a number input: if the pin is wired, compiles upstream; otherwise reads the UI field. */
inline Node* BuildNumberOperand(GraphCompiler* compiler, const VisualNode& n, const Pin& pin, const char* fieldName = nullptr)
{
    if (compiler->Resolve(pin))
        return compiler->BuildExpr(pin);

    const std::string* fieldValue = FindInputFallbackField(n, pin, fieldName);
    try { return MakeNumberLiteral(fieldValue ? std::stod(*fieldValue) : 0.0); }
    catch (...) { return MakeNumberLiteral(0.0); }
}

/** Build a bool input: if the pin is wired, compiles upstream; otherwise reads the UI field. */
inline Node* BuildBoolOperand(GraphCompiler* compiler, const VisualNode& n, const Pin& pin, const char* fieldName = nullptr)
{
    if (compiler->Resolve(pin))
        return compiler->BuildExpr(pin);

    const std::string* fieldValue = FindInputFallbackField(n, pin, fieldName);
    const std::string value = fieldValue ? *fieldValue : "false";
    return MakeBoolLiteral(value == "true" || value == "True" || value == "1");
}

/** Build an array input: if the pin is wired, compiles upstream; otherwise parses the UI field as an array literal. */
inline Node* BuildArrayInput(GraphCompiler* compiler, const VisualNode& n, const Pin& pin, const char* fieldName = nullptr)
{
    if (compiler->Resolve(pin))
        return compiler->BuildExpr(pin);

    const std::string* fieldValue = FindInputFallbackField(n, pin, fieldName);
    return compiler->BuildArrayLiteralNode(fieldValue ? *fieldValue : "[]");
}

/** Unique string suffix derived from the node's id — used when generating internal variable names. */
inline std::string NodeInstanceSuffix(const VisualNode& n)
{
    return std::to_string(static_cast<unsigned long long>(static_cast<uintptr_t>(n.id.Get())));
}

/** Internal variable names for Loop nodes (unique per node instance). */
inline std::string LoopIndexVarNameForNode(const VisualNode& n)    { return "__loop_i_"      + NodeInstanceSuffix(n); }
inline std::string LoopLastIndexVarNameForNode(const VisualNode& n) { return "__loop_last_i_" + NodeInstanceSuffix(n); }
inline std::string LoopStartVarNameForNode(const VisualNode& n)    { return "__loop_start_i_"+ NodeInstanceSuffix(n); }
inline std::string LoopEndVarNameForNode(const VisualNode& n)      { return "__loop_end_i_"  + NodeInstanceSuffix(n); }

/** Internal variable name for the current element in a ForEach body. */
inline std::string ForEachElementVarNameForNode(const VisualNode& n) { return "__foreach_elem_" + NodeInstanceSuffix(n); }

} // namespace
