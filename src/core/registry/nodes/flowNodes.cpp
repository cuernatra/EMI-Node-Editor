#include "../nodeRegistry.h"
#include "nodeCompileHelpers.h"

namespace
{
Node* CompileDelayNode(GraphCompiler* compiler, const VisualNode& n)
{
    Node* durationExpr = nullptr;
    const Pin* durationPin = FindInputPin(n, "Duration");
    if (durationPin)
    {
        if (const PinSource* src = compiler->Resolve(*durationPin))
        {
            durationExpr = compiler->BuildNode(*src->node, src->pinIdx);
            if (compiler->HasError) return nullptr;
        }
    }
    if (!durationExpr)
    {
        const std::string* durStr = FindField(n, "Duration");
        double ms = 1000.0;
        if (durStr) { try { ms = std::stod(*durStr); } catch (...) { ms = 1000.0; } }
        durationExpr = MakeNumberLiteral(ms);
    }
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
}

void NodeRegistry::RegisterFlowNodes()
{
    // Register(...) fields follow NodeDescriptor member order.
    Register({
        NodeType::Delay,
        "Delay",
        {
            { "In", PinType::Flow, true },
            { "Duration", PinType::Number, true },
            { "Out", PinType::Flow, false }
        },
        {
            { "Duration", PinType::Number, "1000.0" }
        },
        CompileDelayNode,
        nullptr,
        "Flow",
        {},
        "Delay",
        { "Duration" },
        NodeRenderStyle::Delay
    });

    Register({
        NodeType::Sequence,
        "Sequence",
        {
            { "In", PinType::Flow, true },
            { "Then 0", PinType::Flow, false }
        },
        {},
        CompileSequenceNode,
        DeserializeSequenceNode,
        "Flow",
        {},
        "Sequence",
        {},
        NodeRenderStyle::Sequence
    });

    Register({
        NodeType::Branch,
        "Branch",
        {
            { "In", PinType::Flow, true },
            { "Condition", PinType::Boolean, true },
            { "True", PinType::Flow, false },
            { "False", PinType::Flow, false }
        },
        {},
        CompileBranchNode,
        nullptr,
        "Flow",
        {},
        "Branch",
        {},
        NodeRenderStyle::Default
    });

    Register({
        NodeType::Loop,
        "Loop",
        {
            { "In", PinType::Flow, true },
            { "Start", PinType::Number, true },
            { "Count", PinType::Number, true },
            { "Body", PinType::Flow, false },
            { "Completed", PinType::Flow, false },
            { "Index", PinType::Number, false }
        },
        {
            { "Start", PinType::Number, "0" },
            { "Count", PinType::Number, "0" }
        },
        CompileLoopNode,
        nullptr,
        "Flow",
        {},
        "Loop",
        { "Start", "Count" },
        NodeRenderStyle::Loop
    });

    Register({
        NodeType::ForEach,
        "For Each",
        {
            { "In", PinType::Flow, true },
            { "Array", PinType::Array, true },
            { "Body", PinType::Flow, false },
            { "Completed", PinType::Flow, false },
            { "Element", PinType::Any, false }
        },
        {
            { "Element Type", PinType::String, "Any", { "Any", "Number", "Boolean", "String", "Array" } },
            { "Array", PinType::Array, "[]" }
        },
        CompileForEachNode,
        nullptr,
        "Flow",
        {},
        "ForEach",
        {},
        NodeRenderStyle::ForEach
    });

    Register({
        NodeType::While,
        "While",
        {
            { "In", PinType::Flow, true },
            { "Condition", PinType::Boolean, true },
            { "Body", PinType::Flow, false },
            { "Completed", PinType::Flow, false }
        },
        {},
        CompileWhileNode,
        nullptr,
        "Flow",
        {},
        "While",
        {},
        NodeRenderStyle::Default
    });
}
