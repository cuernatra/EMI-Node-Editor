#include "../nodeRegistry.h"
#include "nodeCompileHelpers.h"



// Move CompileTickerNode out of anonymous namespace for linker visibility
Node* CompileTickerNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* fpsPin = FindInputPin(n, "FPS");
    if (!fpsPin)
    {
        compiler->Error("Ticker node needs FPS input");
        return nullptr;
    }

    Node* fps = BuildNumberOperand(compiler, n, *fpsPin);
    if (!fps)
        return nullptr;

    Node* body = MakeNode(Token::Scope);
    Node* loop = MakeNode(Token::While);
    loop->children.push_back(MakeBoolLiteral(true));
    loop->children.push_back(body);

    if (const Pin* tick = compiler->GetOutputPinByName(n, "Tick"))
        compiler->AppendFlowChainFromOutput(tick->id, body);

    // Delay appended after user's body chain, inside the loop scope.
    body->children.push_back(
        compiler->EmitFunctionCall("delay", { compiler->EmitBinaryOp(Token::Div, MakeNumberLiteral(1000.0), fps) })
    );

    Node* targetScope = MakeNode(Token::Scope);
    targetScope->children.push_back(loop);

    if (const Pin* stop = compiler->GetOutputPinByName(n, "Stop"))
        compiler->AppendFlowChainFromOutput(stop->id, targetScope);

    return targetScope;
}

// End anonymous namespace


Node* CompileDelayNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* durationPin = FindInputPin(n, "Duration");
    if (!durationPin)
    {
        compiler->Error("Delay node needs Duration input");
        return nullptr;
    }

    Node* durationExpr = BuildNumberOperand(compiler, n, *durationPin);
    if (!durationExpr)
        return nullptr;

    return compiler->EmitFunctionCall("delay", { durationExpr });
}

Node* CompileSequenceNode(GraphCompiler*, const VisualNode&)
{
    return MakeNode(Token::Scope);
}

bool DeserializeSequenceNode(VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds)
{
    if (pinIds.size() < desc.pins.size())
        return false;

    std::vector<int> basePins(pinIds.begin(), pinIds.begin() + static_cast<std::ptrdiff_t>(desc.pins.size()));
    if (!PopulateExactPinsAndFields(n, desc, basePins))
        return false;

    for (size_t i = desc.pins.size(); i < pinIds.size(); ++i)
    {
        const int thenIndex = static_cast<int>(n.outPins.size());
        n.outPins.push_back(MakePin(
            static_cast<uint32_t>(pinIds[i]),
            n.id,
            desc.type,
            "Then " + std::to_string(thenIndex),
            PinType::Flow,
            false
        ));
    }

    return true;
}

bool DeserializeCallFunctionNode(VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds)
{
    if (pinIds.size() < desc.pins.size())
        return false;

    std::vector<int> basePins(pinIds.begin(), pinIds.begin() + static_cast<std::ptrdiff_t>(desc.pins.size()));
    if (!PopulateExactPinsAndFields(n, desc, basePins))
        return false;

    // Extra pins are argument inputs.
    for (size_t i = desc.pins.size(); i < pinIds.size(); ++i)
    {
        const int argIndex = static_cast<int>(i - desc.pins.size());
        n.inPins.push_back(MakePin(
            static_cast<uint32_t>(pinIds[i]),
            n.id,
            desc.type,
            "Arg" + std::to_string(argIndex),
            PinType::Any,
            true
        ));
    }

    return true;
}

Node* CompileBranchNode(GraphCompiler* compiler, const VisualNode& n)
{
    return BuildBranchNode(compiler, n);
}

Node* CompileLoopNode(GraphCompiler* compiler, const VisualNode& n)
{
    return BuildLoopNode(compiler, n);
}

Node* CompileForEachNode(GraphCompiler* compiler, const VisualNode& n)
{
    return BuildForEachNode(compiler, n);
}

Node* CompileWhileNode(GraphCompiler* compiler, const VisualNode& n)
{
    return BuildWhileNode(compiler, n);
}

void NodeRegistry::RegisterFlowNodes()
{

    Register(NodeDescriptor{
        .type = NodeType::Ticker,
        .label = "Ticker",
        .pins = {
            { "In", PinType::Flow, true },
            { "FPS", PinType::Number, true },
            { "Tick", PinType::Flow, false },
            { "Stop", PinType::Flow, false }
        },
        .fields = {
            { "FPS", PinType::Number, "20" }
        },
        .compile = CompileTickerNode,
        .deserialize = nullptr,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "Ticker",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Default,
    });

    Register(NodeDescriptor{
        .type = NodeType::Delay,
        .label = "Delay",
        .pins = {
            { "In", PinType::Flow, true },
            { "Duration", PinType::Number, true },
            { "Out", PinType::Flow, false }
        },
        .fields = {
            { "Duration", PinType::Number, "1000.0" }
        },
        .compile = CompileDelayNode,
        .deserialize = nullptr,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "Delay",
        .deferredInputPins = { "Duration" },
        .renderStyle = NodeRenderStyle::Delay,
    });

    Register(NodeDescriptor{
        .type = NodeType::Sequence,
        .label = "Sequence",
        .pins = {
            { "In", PinType::Flow, true },
            { "Then 0", PinType::Flow, false }
        },
        .fields = {},
        .compile = CompileSequenceNode,
        .deserialize = DeserializeSequenceNode,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "Sequence",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Sequence,
    });

    Register(NodeDescriptor{
        .type = NodeType::Branch,
        .label = "Branch",
        .pins = {
            { "In", PinType::Flow, true },
            { "Condition", PinType::Boolean, true },
            { "True", PinType::Flow, false },
            { "False", PinType::Flow, false }
        },
        .fields = {},
        .compile = CompileBranchNode,
        .deserialize = nullptr,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "Branch",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Default,
    });

    Register(NodeDescriptor{
        .type = NodeType::CallFunction,
        .label = "Call Function",
        .pins = {
            { "In", PinType::Flow, true },
            { "Out", PinType::Flow, false },
            { "Result", PinType::Any, false }
        },
        .fields = {
            { "Name", PinType::String, "" }
        },
        .compile = nullptr,
        .deserialize = DeserializeCallFunctionNode,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "CallFunction",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Default,
    });

    Register(NodeDescriptor{
        .type = NodeType::Loop,
        .label = "Loop",
        .pins = {
            { "In", PinType::Flow, true },
            { "Start", PinType::Number, true },
            { "Count", PinType::Number, true },
            { "Body", PinType::Flow, false },
            { "Completed", PinType::Flow, false },
            { "Index", PinType::Number, false }
        },
        .fields = {
            { "Start", PinType::Number, "0" },
            { "Count", PinType::Number, "0" }
        },
        .compile = CompileLoopNode,
        .deserialize = nullptr,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "Loop",
        .deferredInputPins = { "Start", "Count" },
        .renderStyle = NodeRenderStyle::Loop,
    });

    Register(NodeDescriptor{
        .type = NodeType::ForEach,
        .label = "For Each",
        .pins = {
            { "In", PinType::Flow, true },
            { "Array", PinType::Array, true },
            { "Body", PinType::Flow, false },
            { "Completed", PinType::Flow, false },
            { "Element", PinType::Any, false }
        },
        .fields = {
            { "Element Type", PinType::String, "Any", { "Any", "Number", "Boolean", "String", "Array" } },
            { "Array", PinType::Array, "[]" }
        },
        .compile = CompileForEachNode,
        .deserialize = nullptr,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "ForEach",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::ForEach,
    });

    Register(NodeDescriptor{
        .type = NodeType::While,
        .label = "While",
        .pins = {
            { "In", PinType::Flow, true },
            { "Condition", PinType::Boolean, true },
            { "Body", PinType::Flow, false },
            { "Completed", PinType::Flow, false }
        },
        .fields = {},
        .compile = CompileWhileNode,
        .deserialize = nullptr,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "While",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Default,
    });
}
