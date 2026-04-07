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
    Register(NodeDescriptor{
        .type = NodeType::Start,
        .label = "Start",
        .pins = {
            { "Exec", PinType::Flow, false }
        },
        .fields = {},
        .compile = CompileScopeNode,
        .deserialize = nullptr,
        .category = "Events",
        .paletteVariants = {},
        .saveToken = "Start",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Default,
    });

    Register(NodeDescriptor{
        .type = NodeType::DrawRect,
        .label = "Draw Rect",
        .pins = {
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
        .fields = {
            { "Color", PinType::String, "120, 180, 255" },
            { "X", PinType::Number, "0.0" },
            { "Y", PinType::Number, "0.0" },
            { "W", PinType::Number, "1.0" },
            { "H", PinType::Number, "1.0" },
            { "R", PinType::Number, "120.0" },
            { "G", PinType::Number, "180.0" },
            { "B", PinType::Number, "255.0" }
        },
        .compile = CompileScopeNode,
        .deserialize = nullptr,
        .category = "Events",
        .paletteVariants = {},
        .saveToken = "DrawRect",
        .deferredInputPins = { "X", "Y", "W", "H" },
        .renderStyle = NodeRenderStyle::Draw,
    });

    Register(NodeDescriptor{
        .type = NodeType::DrawGrid,
        .label = "Draw Grid",
        .pins = {
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
        .fields = {
            { "Color", PinType::String, "30, 30, 38" },
            { "X", PinType::Number, "0.0" },
            { "Y", PinType::Number, "0.0" },
            { "W", PinType::Number, "40.0" },
            { "H", PinType::Number, "60.0" },
            { "R", PinType::Number, "30.0" },
            { "G", PinType::Number, "30.0" },
            { "B", PinType::Number, "38.0" }
        },
        .compile = CompileScopeNode,
        .deserialize = nullptr,
        .category = "Events",
        .paletteVariants = {},
        .saveToken = "DrawGrid",
        .deferredInputPins = { "X", "Y", "W", "H" },
        .renderStyle = NodeRenderStyle::Draw,
    });

    Register(NodeDescriptor{
        .type = NodeType::Function,
        .label = "Function",
        .pins = {
            { "Out", PinType::Flow, false }
        },
        .fields = {
            { "Name", PinType::String, "myFunction" }
        },
        .compile = nullptr,
        .deserialize = DeserializeFunctionNode,
        .category = "Events",
        .paletteVariants = {},
        .saveToken = "Function",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Default,
    });
}
