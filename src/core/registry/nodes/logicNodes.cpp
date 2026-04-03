#include "../nodeRegistry.h"
#include "nodeCompileHelpers.h"

namespace
{
Node* CompileOperatorNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* pinA = FindInputPin(n, "A");
    const Pin* pinB = FindInputPin(n, "B");
    if (!pinA || !pinB)
    {
        compiler->Error("Operator node needs A and B inputs");
        return nullptr;
    }

    const std::string* opStr = FindField(n, "Op");
    const Token tok = ParseOperatorToken(opStr);
    if (tok == Token::None)
    {
        compiler->Error("Unknown operator: " + (opStr ? *opStr : "?"));
        return nullptr;
    }

    Node* lhs = BuildNumberOperand(compiler, n, *pinA, "A");
    Node* rhs = BuildNumberOperand(compiler, n, *pinB, "B");
    return compiler->EmitBinaryOp(tok, lhs, rhs);
}

Node* CompileComparisonNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* pinA = FindInputPin(n, "A");
    const Pin* pinB = FindInputPin(n, "B");
    if (!pinA || !pinB)
    {
        compiler->Error("Comparison node needs A and B inputs");
        return nullptr;
    }

    const std::string* opStr = FindField(n, "Op");
    const Token tok = ParseComparisonToken(opStr);
    if (tok == Token::None)
    {
        compiler->Error("Unknown comparison: " + (opStr ? *opStr : "?"));
        return nullptr;
    }

    Node* lhs = BuildNumberOperand(compiler, n, *pinA, "A");
    Node* rhs = BuildNumberOperand(compiler, n, *pinB, "B");
    return compiler->EmitBinaryOp(tok, lhs, rhs);
}

Node* CompileLogicNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* pinA = FindInputPin(n, "A");
    const Pin* pinB = FindInputPin(n, "B");
    if (!pinA || !pinB)
    {
        compiler->Error("Logic node needs A and B inputs");
        return nullptr;
    }

    const std::string* opStr = FindField(n, "Op");
    const Token tok = ParseLogicToken(opStr);
    if (tok == Token::None)
    {
        compiler->Error("Unknown logic op: " + (opStr ? *opStr : "?"));
        return nullptr;
    }

    Node* lhs = BuildBoolOperand(compiler, n, *pinA, "A");
    Node* rhs = BuildBoolOperand(compiler, n, *pinB, "B");
    return compiler->EmitBinaryOp(tok, lhs, rhs);
}

Node* CompileNotNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* pinA = FindInputPin(n, "A");
    if (!pinA)
    {
        compiler->Error("Not node needs A input");
        return nullptr;
    }

    Node* operand = BuildBoolOperand(compiler, n, *pinA, "A");
    return compiler->EmitUnaryOp(Token::Not, operand);
}
}

void NodeRegistry::RegisterLogicNodes()
{
    // Each Register(...) entry below follows NodeDescriptor order:
    // type, label, pins, fields, compile, deserialize, category, paletteVariants, saveToken.
    Register({
        NodeType::Operator,
        "Operator",
        {
            { "In", PinType::Flow, true },
            { "A", PinType::Number, true },
            { "B", PinType::Number, true },
            { "Out", PinType::Flow, false },
            { "Result", PinType::Number, false }
        },
        {
            { "Op", PinType::String, "+", { "+", "-", "*", "/" } },
            { "A", PinType::Number, "0.0" },
            { "B", PinType::Number, "0.0" }
        },
        CompileOperatorNode,
        nullptr,
        "Flow",
        {},
        "Operator"
    });

    Register({
        NodeType::Comparison,
        "Compare",
        {
            { "In", PinType::Flow, true },
            { "A", PinType::Number, true },
            { "B", PinType::Number, true },
            { "Out", PinType::Flow, false },
            { "Result", PinType::Boolean, false }
        },
        {
            { "Op", PinType::String, ">=", { "==", "!=", "<", "<=", ">", ">=" } },
            { "A", PinType::Number, "0.0" },
            { "B", PinType::Number, "0.0" }
        },
        CompileComparisonNode,
        nullptr,
        "Logic",
        {},
        "Comparison"
    });

    Register({
        NodeType::Logic,
        "Logic",
        {
            { "In", PinType::Flow, true, false },
            { "A", PinType::Boolean, true, false },
            { "B", PinType::Boolean, true, false },
            { "Out", PinType::Flow, false, false },
            { "Result", PinType::Boolean, false }
        },
        {
            { "Op", PinType::String, "AND", { "AND", "OR" } },
            { "A", PinType::Boolean, "false" },
            { "B", PinType::Boolean, "false" }
        },
        CompileLogicNode,
        nullptr,
        "Logic",
        {},
        "Logic"
    });

    Register({
        NodeType::Not,
        "Not",
        {
            { "In", PinType::Flow, true },
            { "A", PinType::Boolean, true },
            { "Out", PinType::Flow, false },
            { "Result", PinType::Boolean, false }
        },
        {
            { "A", PinType::Boolean, "false" }
        },
        CompileNotNode,
        nullptr,
        "Logic",
        {},
        "Not"
    });
}
