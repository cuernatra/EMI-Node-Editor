#include "../nodeRegistry.h"
#include "../../compiler/graphCompiler.h"
#include "../../graph/visualNode.h"

namespace
{
bool PopulateExactPinsAndFields(VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds)
{
    if (pinIds.size() != desc.pins.size())
        return false;

    size_t pinIndex = 0;
    for (const PinDescriptor& pd : desc.pins)
    {
        const uint32_t pinId = static_cast<uint32_t>(pinIds[pinIndex++]);
        Pin p = MakePin(pinId, n.id, desc.type, pd.name, pd.type, pd.isInput, pd.isMultiInput);
        if (pd.isInput)
            n.inPins.push_back(p);
        else
            n.outPins.push_back(p);
    }

    for (const FieldDescriptor& fd : desc.fields)
        n.fields.push_back(MakeNodeField(fd));

    return true;
}
}

void NodeRegistry::RegisterFlowNodes()
{
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
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildDelay(n); },
        nullptr,
        "Flow"
    });

    Register({
        NodeType::Sequence,
        "Sequence",
        {
            { "In", PinType::Flow, true },
            { "Then 0", PinType::Flow, false }
        },
        {},
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildSequence(n); },
        [](VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds) -> bool {
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
        },
        "Flow"
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
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildBranch(n); },
        nullptr,
        "Flow"
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
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildLoop(n); },
        nullptr,
        "Flow"
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
            { "Element Type", PinType::String, "Any" },
            { "Array", PinType::Array, "[]" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildForEach(n); },
        nullptr,
        "Flow"
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
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildWhile(n); },
        nullptr,
        "Flow"
    });
}
