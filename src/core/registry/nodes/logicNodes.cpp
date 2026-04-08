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
    return MakeBinaryOpNode(tok, lhs, rhs);
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
    return MakeBinaryOpNode(tok, lhs, rhs);
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
    return MakeBinaryOpNode(tok, lhs, rhs);
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
    return MakeUnaryOpNode(Token::Not, operand);
}

Node* CompileMathUnaryNode(GraphCompiler* compiler, const VisualNode& n)
{
    const std::string* opStr = FindField(n, "Op");
    std::string funcName;
    if (!opStr || *opStr == "Sqrt")       funcName = "Math.Sqrt";
    else if (*opStr == "Floor")           funcName = "Math.Floor";
    else if (*opStr == "Round")           funcName = "Math.Round";
    else if (*opStr == "Abs")             funcName = "Math.Abs";
    else
    {
        compiler->Error("Unknown MathUnary op: " + (opStr ? *opStr : "?"));
        return nullptr;
    }

    const Pin* valuePin = FindInputPin(n, "Value");
    if (!valuePin)
    {
        compiler->Error("MathUnary node needs Value input");
        return nullptr;
    }

    Node* valueExpr = BuildNumberOperand(compiler, n, *valuePin);
    return MakeFunctionCallNode(funcName, { valueExpr });
}

Node* CompileMathBinaryNode(GraphCompiler* compiler, const VisualNode& n)
{
    const std::string* opStr = FindField(n, "Op");
    std::string funcName;
    if (!opStr || *opStr == "Max")        funcName = "Math.Max";
    else if (*opStr == "Min")             funcName = "Math.Min";
    else
    {
        compiler->Error("Unknown MathBinary op: " + (opStr ? *opStr : "?"));
        return nullptr;
    }

    const Pin* pinA = FindInputPin(n, "A");
    const Pin* pinB = FindInputPin(n, "B");
    if (!pinA || !pinB)
    {
        compiler->Error("MathBinary node needs A and B inputs");
        return nullptr;
    }

    Node* aExpr = BuildNumberOperand(compiler, n, *pinA);
    Node* bExpr = BuildNumberOperand(compiler, n, *pinB);
    return MakeFunctionCallNode(funcName, { aExpr, bExpr });
}

Node* CompileMathClampNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* valuePin = FindInputPin(n, "Value");
    const Pin* minPin   = FindInputPin(n, "Min");
    const Pin* maxPin   = FindInputPin(n, "Max");
    if (!valuePin || !minPin || !maxPin)
    {
        compiler->Error("MathClamp node needs Value, Min and Max inputs");
        return nullptr;
    }

    Node* valueExpr = BuildNumberOperand(compiler, n, *valuePin);
    Node* minExpr   = BuildNumberOperand(compiler, n, *minPin);
    Node* maxExpr   = BuildNumberOperand(compiler, n, *maxPin);
    return MakeFunctionCallNode("Math.Clamp", { valueExpr, minExpr, maxExpr });
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

    Register(NodeDescriptor{
        .type = NodeType::MathUnary,
        .label = "Math Unary",
        .pins = {
            PinDescriptor{ .name = "Value", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Result", .type = PinType::Number, .isInput = false, .isMultiInput = false }
        },
        .fields = {
            FieldDescriptor{ .name = "Op", .valueType = PinType::String, .defaultValue = "Sqrt", .options = { "Sqrt", "Floor", "Round", "Abs" } },
            FieldDescriptor{ .name = "Value", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} }
        },
        .compile = CompileMathUnaryNode,
        .deserialize = nullptr,
        .category = "Math",
        .paletteVariants = {},
        .saveToken = "MathUnary",
        .deferredInputPins = { "Value" },
        .renderStyle = NodeRenderStyle::Unary
    });

    Register(NodeDescriptor{
        .type = NodeType::MathBinary,
        .label = "Math Binary",
        .pins = {
            PinDescriptor{ .name = "A", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "B", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Result", .type = PinType::Number, .isInput = false, .isMultiInput = false }
        },
        .fields = {
            FieldDescriptor{ .name = "Op", .valueType = PinType::String, .defaultValue = "Max", .options = { "Max", "Min" } },
            FieldDescriptor{ .name = "A", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} },
            FieldDescriptor{ .name = "B", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} }
        },
        .compile = CompileMathBinaryNode,
        .deserialize = nullptr,
        .category = "Math",
        .paletteVariants = {},
        .saveToken = "MathBinary",
        .deferredInputPins = { "A", "B" },
        .renderStyle = NodeRenderStyle::Binary
    });

    Register(NodeDescriptor{
        .type = NodeType::MathClamp,
        .label = "Math Clamp",
        .pins = {
            PinDescriptor{ .name = "Value", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Min", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Max", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Result", .type = PinType::Number, .isInput = false, .isMultiInput = false }
        },
        .fields = {
            FieldDescriptor{ .name = "Value", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} },
            FieldDescriptor{ .name = "Min", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} },
            FieldDescriptor{ .name = "Max", .valueType = PinType::Number, .defaultValue = "1.0", .options = {} }
        },
        .compile = CompileMathClampNode,
        .deserialize = nullptr,
        .category = "Math",
        .paletteVariants = {},
        .saveToken = "MathClamp",
        .deferredInputPins = { "Value", "Min", "Max" },
        .renderStyle = NodeRenderStyle::Default
    });
}
