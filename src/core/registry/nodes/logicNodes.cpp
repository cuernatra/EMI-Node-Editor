#include "../nodeRegistry.h"
#include "../../compiler/nodeCompileHelpers.h"

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

    Node* lhs = BuildNumberOperand(compiler, n, *pinA);
    Node* rhs = BuildNumberOperand(compiler, n, *pinB);
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

    Node* lhs = BuildNumberOperand(compiler, n, *pinA);
    Node* rhs = BuildNumberOperand(compiler, n, *pinB);
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

    Node* lhs = BuildBoolOperand(compiler, n, *pinA);
    Node* rhs = BuildBoolOperand(compiler, n, *pinB);
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

    Node* operand = BuildBoolOperand(compiler, n, *pinA);
    return compiler->EmitUnaryOp(Token::Not, operand);
}
}

void NodeRegistry::RegisterLogicNodes()
{
    Register(NodeDescriptor{
        .type = NodeType::Operator,
        .label = "Operator",
        .pins = {
            PinDescriptor{ .name = "In", .type = PinType::Flow, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "A", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "B", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Out", .type = PinType::Flow, .isInput = false, .isMultiInput = false },
            PinDescriptor{ .name = "Result", .type = PinType::Number, .isInput = false, .isMultiInput = false }
        },
        .fields = {
            FieldDescriptor{ .name = "Op", .valueType = PinType::String, .defaultValue = "+", .options = { "+", "-", "*", "/" } },
            FieldDescriptor{ .name = "A", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} },
            FieldDescriptor{ .name = "B", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} }
        },
        .compile = CompileOperatorNode,
        .deserialize = nullptr,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "Operator",
        .deferredInputPins = { "A", "B" },
        .renderStyle = NodeRenderStyle::Binary
    });

    Register(NodeDescriptor{
        .type = NodeType::Comparison,
        .label = "Compare",
        .pins = {
            PinDescriptor{ .name = "In", .type = PinType::Flow, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "A", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "B", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Out", .type = PinType::Flow, .isInput = false, .isMultiInput = false },
            PinDescriptor{ .name = "Result", .type = PinType::Boolean, .isInput = false, .isMultiInput = false }
        },
        .fields = {
            FieldDescriptor{ .name = "Op", .valueType = PinType::String, .defaultValue = ">=", .options = { "==", "!=", "<", "<=", ">", ">=" } },
            FieldDescriptor{ .name = "A", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} },
            FieldDescriptor{ .name = "B", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} }
        },
        .compile = CompileComparisonNode,
        .deserialize = nullptr,
        .category = "Logic",
        .paletteVariants = {},
        .saveToken = "Comparison",
        .deferredInputPins = { "A", "B" },
        .renderStyle = NodeRenderStyle::Binary
    });

    Register(NodeDescriptor{
        .type = NodeType::Logic,
        .label = "Logic",
        .pins = {
            PinDescriptor{ .name = "In", .type = PinType::Flow, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "A", .type = PinType::Boolean, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "B", .type = PinType::Boolean, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Out", .type = PinType::Flow, .isInput = false, .isMultiInput = false },
            PinDescriptor{ .name = "Result", .type = PinType::Boolean, .isInput = false, .isMultiInput = false }
        },
        .fields = {
            FieldDescriptor{ .name = "Op", .valueType = PinType::String, .defaultValue = "AND", .options = { "AND", "OR" } },
            FieldDescriptor{ .name = "A", .valueType = PinType::Boolean, .defaultValue = "false", .options = {} },
            FieldDescriptor{ .name = "B", .valueType = PinType::Boolean, .defaultValue = "false", .options = {} }
        },
        .compile = CompileLogicNode,
        .deserialize = nullptr,
        .category = "Logic",
        .paletteVariants = {},
        .saveToken = "Logic",
        .deferredInputPins = { "A", "B" },
        .renderStyle = NodeRenderStyle::Binary
    });

    Register(NodeDescriptor{
        .type = NodeType::Not,
        .label = "Not",
        .pins = {
            PinDescriptor{ .name = "In", .type = PinType::Flow, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "A", .type = PinType::Boolean, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Out", .type = PinType::Flow, .isInput = false, .isMultiInput = false },
            PinDescriptor{ .name = "Result", .type = PinType::Boolean, .isInput = false, .isMultiInput = false }
        },
        .fields = {
            FieldDescriptor{ .name = "A", .valueType = PinType::Boolean, .defaultValue = "false", .options = {} }
        },
        .compile = CompileNotNode,
        .deserialize = nullptr,
        .category = "Logic",
        .paletteVariants = {},
        .saveToken = "Not",
        .deferredInputPins = { "A" },
        .renderStyle = NodeRenderStyle::Unary
    });
}
