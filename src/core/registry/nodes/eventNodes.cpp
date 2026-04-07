#include "../nodeRegistry.h"
#include "nodeCompileHelpers.h"

namespace
{
Node* CompileScopeNode(GraphCompiler*, const VisualNode&)
{
    return MakeNode(Token::Scope);
}

bool DeserializeFunctionNode(VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds)
{
    if (pinIds.size() < desc.pins.size())
        return false;

    std::vector<int> basePins(pinIds.begin(), pinIds.begin() + static_cast<std::ptrdiff_t>(desc.pins.size()));
    if (!PopulateExactPinsAndFields(n, desc, basePins))
        return false;

    for (size_t i = desc.pins.size(); i < pinIds.size(); ++i)
    {
        const int paramIndex = static_cast<int>(i - desc.pins.size());
        n.outPins.push_back(MakePin(
            static_cast<uint32_t>(pinIds[i]),
            n.id,
            desc.type,
            "Param" + std::to_string(paramIndex),
            PinType::Any,
            false
        ));
    }

    return true;
}
}

void NodeRegistry::RegisterEventNodes()
{
    // Register(...) fields follow NodeDescriptor member order.
    Register({
        NodeType::Start,
        "Start",
        {
            { "Exec", PinType::Flow, false }
        },
        {},
        CompileScopeNode,
        nullptr,
        "Events",
        {},
        "Start"
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
        "Events",
        {},
        "DrawRect",
        { "X", "Y", "W", "H" },
        NodeRenderStyle::Draw
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
        "Events",
        {},
        "DrawGrid",
        { "X", "Y", "W", "H" },
        NodeRenderStyle::Draw
    });

    Register({
        NodeType::Function,
        "Function",
        {
            { "Out", PinType::Flow, false }
        },
        {
            { "Name", PinType::String, "myFunction" }
        },
        nullptr,
        DeserializeFunctionNode,
        "Events",
        {},
        "Function",
        {},
        NodeRenderStyle::Default
    });
}
