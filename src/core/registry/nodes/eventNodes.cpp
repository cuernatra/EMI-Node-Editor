#include "../nodeRegistry.h"
#include "../../compiler/graphCompiler.h"

void NodeRegistry::RegisterEventNodes()
{
    Register({
        NodeType::Start,
        "Start",
        {
            { "Exec", PinType::Flow, false }
        },
        {},
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildStart(n); },
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
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildDrawRect(n); },
        nullptr,
        "Flow"
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
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildDrawGrid(n); },
        nullptr,
        "Logic"
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
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildFunction(n); },
        nullptr,
        "Logic"
    });
}
