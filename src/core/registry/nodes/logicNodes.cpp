#include "../nodeRegistry.h"
#include "../../compiler/graphCompiler.h"

void NodeRegistry::RegisterLogicNodes()
{
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
            { "Op", PinType::String, "+" },
            { "A", PinType::Number, "0.0" },
            { "B", PinType::Number, "0.0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildOperator(n); },
        nullptr,
        "Logic"
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
            { "Op", PinType::String, ">=" },
            { "A", PinType::Number, "0.0" },
            { "B", PinType::Number, "0.0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildComparison(n); },
        nullptr,
        "Logic"
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
            { "Op", PinType::String, "AND" },
            { "A", PinType::Boolean, "false" },
            { "B", PinType::Boolean, "false" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildLogic(n); },
        nullptr,
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
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildNot(n); },
        nullptr,
        "Logic"
    });
}
