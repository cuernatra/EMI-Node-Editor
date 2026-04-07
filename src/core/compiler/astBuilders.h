#pragma once

#include "Parser/Node.h"
#include "Parser/Lexer.h"

#include <string>

namespace emi::ast
{
/// Create a new AST node of the given token type.
///
/// Ownership: caller owns the returned node until it is attached as a child of
/// another node.
///
/// @param t Token type for the node.
/// @return Newly allocated AST node.
inline Node* MakeNode(Token t)
{
    Node* node = new Node();
    node->type = t;
    return node;
}

/// Create a numeric literal AST node.
///
/// Ownership: caller owns the returned node.
///
/// @param value Numeric value.
/// @return Newly allocated literal node.
inline Node* MakeNumberLiteral(double value)
{
    Node* node = MakeNode(Token::Number);
    node->data = value;
    return node;
}

/// Create a boolean literal AST node.
///
/// Ownership: caller owns the returned node.
///
/// @param value Boolean value.
/// @return Newly allocated literal node.
inline Node* MakeBoolLiteral(bool value)
{
    Node* node = MakeNode(value ? Token::True : Token::False);
    node->data = value;
    return node;
}

/// Create a string literal AST node.
///
/// Ownership: caller owns the returned node.
///
/// @param value Literal string value.
/// @return Newly allocated literal node.
inline Node* MakeStringLiteral(const std::string& value)
{
    Node* node = MakeNode(Token::Literal);
    node->data = value;
    return node;
}

/// Create an identifier AST node.
///
/// Ownership: caller owns the returned node.
///
/// @param name Identifier name.
/// @return Newly allocated identifier node.
inline Node* MakeIdNode(const std::string& name)
{
    Node* node = MakeNode(Token::Id);
    node->data = name;
    return node;
}
} // namespace emi::ast
