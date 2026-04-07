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
 * @file nodeCompileHelpers.h
 * @brief Helper functions for node compile callbacks (graph → AST).
 *
 * Intended for files under `src/core/registry/nodes/`.
 * Discoverability: when working in that folder, you can include the wrapper
 * `src/core/registry/nodes/nodeCompileHelpers.h`.
 *
 * Ownership: most helpers return raw `Node*`. If you return early due to an
 * error, delete any nodes you created that are not attached to a parent yet.
 */

namespace
{
/// Header-only helpers shared by node compile callbacks.

// Re-export the common AST builders so node compile callbacks can use short
// names without duplicating implementations.
using emi::ast::MakeNode;
using emi::ast::MakeIdNode;

/// Populate a node instance with exactly the pins/fields described by a descriptor.
///
/// Used during graph load/deserialization for nodes that have a fixed pin list.
///
/// @param n Node instance to populate (pins/fields are appended).
/// @param desc Static node descriptor.
/// @param pinIds Pin IDs from the saved file.
/// @return True on success; false if `pinIds` count does not match.
inline bool PopulateExactPinsAndFields(VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds)
{
    // Standard load path: pin id count must exactly match descriptor pin count.
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

using emi::ast::MakeNumberLiteral;
using emi::ast::MakeBoolLiteral;
using emi::ast::MakeStringLiteral;

/// Convert an operator string ("+", "-", "*", "/") into an AST token.
///
/// @param opStr Pointer to operator text (may be null).
/// @return Token for the operator, or Token::None if unknown.
inline Token ParseOperatorToken(const std::string* opStr)
{
    if (!opStr || *opStr == "+") return Token::Add;
    if (*opStr == "-") return Token::Sub;
    if (*opStr == "*") return Token::Mult;
    if (*opStr == "/") return Token::Div;
    return Token::None;
}

/// Convert a comparison string ("==", "!=", "<", "<=", ">", ">=") into an AST token.
///
/// @param opStr Pointer to operator text (may be null).
/// @return Token for the comparison, or Token::None if unknown.
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

/// Convert a logic operator string ("AND"/"OR") into an AST token.
///
/// @param opStr Pointer to operator text (may be null).
/// @return Token for the logic operator, or Token::None if unknown.
inline Token ParseLogicToken(const std::string* opStr)
{
    if (!opStr || *opStr == "AND") return Token::And;
    if (*opStr == "OR") return Token::Or;
    return Token::None;
}

/// Convert a variable "Type" field string into a pin type.
///
/// @param typeName Type name string.
/// @return PinType for the variable.
inline PinType VariableTypeFromString(const std::string& typeName)
{
    if (typeName == "Boolean") return PinType::Boolean;
    if (typeName == "String")  return PinType::String;
    if (typeName == "Array")   return PinType::Array;
    return PinType::Number;
}

/// Build a numeric expression for an input pin.
///
/// If the pin is connected, follows the link via `compiler->BuildExpr`.
/// If not, parses a number from a fallback field (defaults to the pin name).
///
/// @param compiler Active compiler.
/// @param n Node instance.
/// @param pin Input pin.
/// @param fieldName Optional field override when disconnected.
/// @return Newly allocated AST node (usually a literal or expression tree).
inline Node* BuildNumberOperand(GraphCompiler* compiler, const VisualNode& n, const Pin& pin, const char* fieldName = nullptr)
{
    if (compiler->Resolve(pin))
        return compiler->BuildExpr(pin);

    const std::string* fieldValue = FindInputFallbackField(n, pin, fieldName);
    try { return MakeNumberLiteral(fieldValue ? std::stod(*fieldValue) : 0.0); }
    catch (...) { return MakeNumberLiteral(0.0); }
}

/// Build a boolean expression for an input pin.
///
/// @param compiler Active compiler.
/// @param n Node instance.
/// @param pin Input pin.
/// @param fieldName Optional field override when disconnected.
/// @return Newly allocated AST node.
inline Node* BuildBoolOperand(GraphCompiler* compiler, const VisualNode& n, const Pin& pin, const char* fieldName = nullptr)
{
    if (compiler->Resolve(pin))
        return compiler->BuildExpr(pin);

    const std::string* fieldValue = FindInputFallbackField(n, pin, fieldName);
    const std::string value = fieldValue ? *fieldValue : "false";
    return MakeBoolLiteral(value == "true" || value == "True" || value == "1");
}

/// Build an array expression for an input pin.
///
/// @param compiler Active compiler.
/// @param n Node instance.
/// @param pin Input pin.
/// @param fieldName Optional field override when disconnected.
/// @return Newly allocated AST node.
inline Node* BuildArrayInput(GraphCompiler* compiler, const VisualNode& n, const Pin& pin, const char* fieldName = nullptr)
{
    if (compiler->Resolve(pin))
        return compiler->BuildExpr(pin);

    const std::string* fieldValue = FindInputFallbackField(n, pin, fieldName);
    return compiler->BuildArrayLiteralNode(fieldValue ? *fieldValue : "[]");
}

/// Build a numeric literal expression from an input pin.
///
/// This differs from BuildNumberOperand mostly in being explicit about returning
/// a numeric literal when disconnected.
///
/// @param compiler Active compiler.
/// @param n Node instance.
/// @param pin Input pin.
/// @param fieldName Optional field override when disconnected.
/// @return Newly allocated numeric literal or expression node.
inline Node* BuildNumberInput(GraphCompiler* compiler, const VisualNode& n, const Pin& pin, const char* fieldName = nullptr)
{
    if (compiler->Resolve(pin))
        return compiler->BuildExpr(pin);

    const std::string* fieldValue = FindInputFallbackField(n, pin, fieldName);
    double parsed = 0.0;
    if (fieldValue)
    {
        try { parsed = std::stod(*fieldValue); }
        catch (...) { parsed = 0.0; }
    }

    return MakeNumberLiteral(parsed);
}

/// Build a literal value based on a (Type, Value) field pair.
///
/// @param compiler Active compiler (used for array literal parsing).
/// @param n Node instance.
/// @param typeFieldName Field containing type name text.
/// @param valueFieldName Field containing value text.
/// @return Newly allocated literal AST node.
inline Node* BuildTypedFieldValue(GraphCompiler* compiler, const VisualNode& n, const char* typeFieldName, const char* valueFieldName)
{
    // Convert a Type + Value field pair into an AST literal node.
    const std::string* typeText = FindField(n, typeFieldName);
    const std::string* valueText = FindField(n, valueFieldName);

    const std::string typeName = typeText ? *typeText : "Number";
    const std::string valueName = valueText ? *valueText : "0.0";

    if (typeName == "Boolean")
        return MakeBoolLiteral(valueName == "true" || valueName == "True" || valueName == "1");

    if (typeName == "String")
        return MakeStringLiteral(valueName);

    if (typeName == "Array")
        return compiler->BuildArrayLiteralNode(valueName);

    try { return MakeNumberLiteral(std::stod(valueName)); }
    catch (...) { return MakeNumberLiteral(0.0); }
}

/// Build the value for a Debug Print / Output-style node.
///
/// Special case: if the node is downstream of a ForEach flow, and the output is
/// not connected, it returns the current ForEach "Element".
/// Otherwise it falls back to a normal `BuildExpr`.
///
/// @param compiler Active compiler.
/// @param n Node instance.
/// @param valuePin The value input pin to evaluate.
/// @return Newly allocated AST node.
inline Node* BuildOutputValue(GraphCompiler* compiler, const VisualNode& n, const Pin& valuePin)
{
    // Build output value from link when present; otherwise fall back to local/default source.
    if (compiler->Resolve(valuePin))
        return compiler->BuildExpr(valuePin);

    const Pin* flowIn = FindInputPin(n, "In");
    if (!flowIn)
        flowIn = FindInputPin(n, "Flow");

    if (flowIn)
    {
        const FlowTarget* flowSrc = compiler->ResolveFlow(*flowIn);
        if (flowSrc && flowSrc->node && flowSrc->node->nodeType == NodeType::ForEach)
        {
            int elementOutIdx = -1;
            for (int i = 0; i < static_cast<int>(flowSrc->node->outPins.size()); ++i)
            {
                const Pin& outPin = flowSrc->node->outPins[static_cast<size_t>(i)];
                if (outPin.type != PinType::Flow && outPin.name == "Element")
                {
                    elementOutIdx = i;
                    break;
                }
            }

            if (elementOutIdx >= 0)
                return compiler->BuildNode(*flowSrc->node, elementOutIdx);
        }
    }

    return compiler->BuildExpr(valuePin);
}

/// Convert plain UI text into an AST string literal.
///
/// @param text Text to embed.
/// @return Newly allocated literal node.
inline Node* ParseTextLiteralNode(const std::string& text)
{
    return MakeStringLiteral(text);
}

/// Stable per-node suffix for generating unique variable names.
///
/// @param n Node instance.
/// @return Stringified node id.
inline std::string NodeInstanceSuffix(const VisualNode& n)
{
    return std::to_string(static_cast<unsigned long long>(static_cast<uintptr_t>(n.id.Get())));
}

/// Variable name used by Loop nodes for the live loop index.
inline std::string LoopIndexVarNameForNode(const VisualNode& n) { return "__loop_i_" + NodeInstanceSuffix(n); }

/// Variable name used by Loop nodes for the cached last index.
inline std::string LoopLastIndexVarNameForNode(const VisualNode& n) { return "__loop_last_i_" + NodeInstanceSuffix(n); }

/// Variable name used by Loop nodes for the computed start index.
inline std::string LoopStartVarNameForNode(const VisualNode& n) { return "__loop_start_i_" + NodeInstanceSuffix(n); }

/// Variable name used by Loop nodes for the computed end index.
inline std::string LoopEndVarNameForNode(const VisualNode& n) { return "__loop_end_i_" + NodeInstanceSuffix(n); }

/// Variable name used by ForEach nodes for the per-iteration element.
inline std::string ForEachElementVarNameForNode(const VisualNode& n) { return "__foreach_elem_" + NodeInstanceSuffix(n); }

} // namespace

