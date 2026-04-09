#pragma once

#include "Parser/Node.h"
#include "Parser/Lexer.h"

#include <string>
#include <vector>

namespace emi::ast
{
/** Create an AST node with the given token type. Caller owns the returned node. */
inline Node* MakeNode(Token t)
{
    Node* node = new Node();
    node->type = t;
    return node;
}

/** Create a number literal node. */
inline Node* MakeNumberLiteral(double value)
{
    Node* node = MakeNode(Token::Number);
    node->data = value;
    return node;
}

/** Create a true/false literal node. */
inline Node* MakeBoolLiteral(bool value)
{
    Node* node = MakeNode(value ? Token::True : Token::False);
    node->data = value;
    return node;
}

/** Create a string literal node. */
inline Node* MakeStringLiteral(const std::string& value)
{
    Node* node = MakeNode(Token::Literal);
    node->data = value;
    return node;
}

/** Create an identifier (variable or function name) node. */
inline Node* MakeIdNode(const std::string& name)
{
    Node* node = MakeNode(Token::Id);
    node->data = name;
    return node;
}

/**
 * Create a binary operator node: lhs <op> rhs.
 * If either side is null, deletes both and returns nullptr.
 *
 * @code
 * Node* result = MakeBinaryOpNode(Token::Add, lhs, rhs);  // lhs + rhs
 * @endcode
 */
inline Node* MakeBinaryOpNode(Token op, Node* lhs, Node* rhs)
{
    if (!lhs || !rhs)
    {
        delete lhs;
        delete rhs;
        return nullptr;
    }

    Node* root = MakeNode(op);
    root->children.push_back(lhs);
    root->children.push_back(rhs);
    return root;
}

/** Create a unary operator node like !x. Deletes operand and returns nullptr if operand is null. */
inline Node* MakeUnaryOpNode(Token op, Node* operand)
{
    if (!operand)
        return nullptr;

    Node* root = MakeNode(op);
    root->children.push_back(operand);
    return root;
}

/**
 * Create a function call node: name(arg1, arg2, ...).
 *
 * @code
 * Node* call = MakeFunctionCallNode("println", { MakeStringLiteral("hi") });
 * @endcode
 */
inline Node* MakeFunctionCallNode(const std::string& name, std::vector<Node*> args)
{
    Node* call = MakeNode(Token::FunctionCall);
    call->children.push_back(MakeIdNode(name));

    Node* params = MakeNode(Token::CallParams);
    for (Node* arg : args)
    {
        if (arg)
            params->children.push_back(arg);
    }

    call->children.push_back(params);
    return call;
}

/** Create an array indexer node: array[index]. Deletes both and returns nullptr if either is null. */
inline Node* MakeIndexerNode(Node* array, Node* index)
{
    if (!array || !index)
    {
        delete array;
        delete index;
        return nullptr;
    }

    Node* idx = MakeNode(Token::Indexer);
    idx->children.push_back(array);
    idx->children.push_back(index);
    return idx;
}
} // namespace emi::ast
