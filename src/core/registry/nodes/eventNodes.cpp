#include "../nodeRegistry.h"
#include "nodeCompileHelpers.h"

namespace
{
Node* CompileScopeNode(GraphCompiler*, const VisualNode&)
{
    return MakeNode(Token::Scope);
}

Node* CompileFunctionNode(GraphCompiler* compiler, const VisualNode& n)
{
    const std::string* nameStr = FindField(n, "Name");
    const std::string funcName = nameStr ? *nameStr : "__fn";

    Node* params = MakeNode(Token::CallParams);
    for (const Pin& pin : n.inPins)
    {
        if (pin.type == PinType::Flow) continue;
        Node* arg = compiler->BuildExpr(pin);
        if (compiler->HasError) { delete params; return nullptr; }
        params->children.push_back(arg);
    }

    Node* call = MakeNode(Token::FunctionCall);
    call->children.push_back(MakeIdNode(funcName));
    call->children.push_back(params);
    return call;
}
}

void NodeRegistry::RegisterEventNodes()
{
    Register({
        NodeType::Start,
        "Start",
        {
            { "Exec", PinType::Flow, false }
        },
        {},
        CompileScopeNode,
        nullptr,
        "Events"
    });

    Register({
        NodeType::DrawRect,
        "Draw Rect",
        {
            { "In", PinType::Flow, true },
            { "X", PinType::Number, true },
            { "Y", PinType::Number, true },
            { "W", PinType::Number, true },
            { "H", PinType::Number, true },
            { "R", PinType::Number, true },
            { "G", PinType::Number, true },
            { "B", PinType::Number, true },
            { "Out", PinType::Flow, false }
        },
        {
            { "Color", PinType::String, "120, 180, 255" },
            { "X", PinType::Number, "0.0" },
            { "Y", PinType::Number, "0.0" },
            { "W", PinType::Number, "1.0" },
            { "H", PinType::Number, "1.0" },
            { "R", PinType::Number, "120.0" },
            { "G", PinType::Number, "180.0" },
            { "B", PinType::Number, "255.0" }
        },
        CompileScopeNode,
        nullptr,
        "Events"
    });

    Register({
        NodeType::DrawGrid,
        "Draw Grid",
        {
            { "In", PinType::Flow, true },
            { "X", PinType::Number, true },
            { "Y", PinType::Number, true },
            { "W", PinType::Number, true },
            { "H", PinType::Number, true },
            { "R", PinType::Number, true },
            { "G", PinType::Number, true },
            { "B", PinType::Number, true },
            { "Out", PinType::Flow, false }
        },
        {
            { "Color", PinType::String, "30, 30, 38" },
            { "X", PinType::Number, "0.0" },
            { "Y", PinType::Number, "0.0" },
            { "W", PinType::Number, "40.0" },
            { "H", PinType::Number, "60.0" },
            { "R", PinType::Number, "30.0" },
            { "G", PinType::Number, "30.0" },
            { "B", PinType::Number, "38.0" }
        },
        CompileScopeNode,
        nullptr,
        "Events"
    });

    Register({
        NodeType::Function,
        "Function",
        {
            { "In", PinType::Flow, true, true },
            { "Out", PinType::Flow, false }
        },
        {
            { "Name", PinType::String, "myFunction" }
        },
        CompileFunctionNode,
        nullptr,
        "Events"
    });
}
