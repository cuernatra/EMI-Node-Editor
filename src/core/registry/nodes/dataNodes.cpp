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

void NodeRegistry::RegisterDataNodes()
{
    Register({
        NodeType::Constant,
        "Constant",
        {
            { "Value", PinType::Any, false }
        },
        {
            { "Type", PinType::String, "Number" },
            { "Value", PinType::Any, "0.0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildConstant(n); },
        nullptr,
        "Data"
    });

    Register({
        NodeType::Variable,
        "Set Variable",
        {
            { "In", PinType::Flow, true },
            { "Default", PinType::Any, true },
            { "Out", PinType::Flow, false }
        },
        {
            { "Variant", PinType::String, "Set" },
            { "Name", PinType::String, "myVar" },
            { "Type", PinType::String, "Number" },
            { "Default", PinType::String, "0.0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildVariable(n); },
        [](VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds) -> bool {
            if (pinIds.size() == 1)
            {
                n.outPins.push_back(MakePin(
                    static_cast<uint32_t>(pinIds[0]),
                    n.id,
                    desc.type,
                    "Value",
                    PinType::Any,
                    false
                ));

                for (const FieldDescriptor& fd : desc.fields)
                    n.fields.push_back(MakeNodeField(fd));

                for (NodeField& f : n.fields)
                {
                    if (f.name == "Variant")
                    {
                        f.value = "Get";
                        break;
                    }
                }
                return true;
            }

            return PopulateExactPinsAndFields(n, desc, pinIds);
        },
        "Data",
        {
            { "Set Variable", "Variable:Set" },
            { "Get Variable", "Variable:Get" }
        }
    });

    Register({
        NodeType::ArrayGetAt,
        "Array Get",
        {
            { "Array", PinType::Array, true },
            { "Index", PinType::Number, true },
            { "Value", PinType::Any, false }
        },
        {
            { "Array", PinType::Array, "[]" },
            { "Index", PinType::Number, "0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildArrayGetAt(n); },
        nullptr,
        "Data"
    });

    Register({
        NodeType::ArrayAddAt,
        "Array Add",
        {
            { "In", PinType::Flow, true },
            { "Array", PinType::Array, true },
            { "Index", PinType::Number, true },
            { "Value", PinType::Any, true },
            { "Out", PinType::Flow, false }
        },
        {
            { "Array", PinType::Array, "[]" },
            { "Index", PinType::Number, "0" },
            { "Add Type", PinType::String, "Number" },
            { "Add Value", PinType::Any, "0.0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildArrayAddAt(n); },
        nullptr,
        "Flow"
    });

    Register({
        NodeType::ArrayReplaceAt,
        "Array Replace",
        {
            { "In", PinType::Flow, true },
            { "Array", PinType::Array, true },
            { "Index", PinType::Number, true },
            { "Value", PinType::Any, true },
            { "Out", PinType::Flow, false }
        },
        {
            { "Array", PinType::Array, "[]" },
            { "Index", PinType::Number, "0" },
            { "Replace Type", PinType::String, "Number" },
            { "Replace Value", PinType::Any, "0.0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildArrayReplaceAt(n); },
        nullptr,
        "Flow"
    });

    Register({
        NodeType::ArrayRemoveAt,
        "Array Remove",
        {
            { "In", PinType::Flow, true },
            { "Array", PinType::Array, true },
            { "Index", PinType::Number, true },
            { "Out", PinType::Flow, false }
        },
        {
            { "Index", PinType::Number, "0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildArrayRemoveAt(n); },
        nullptr,
        "Flow"
    });

    Register({
        NodeType::ArrayLength,
        "Array Length",
        {
            { "Array", PinType::Array, true },
            { "Length", PinType::Number, false }
        },
        {
            { "Array", PinType::Array, "[]" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildArrayLength(n); },
        nullptr,
        "Data"
    });

    Register({
        NodeType::Output,
        "Debug Print",
        {
            { "In", PinType::Flow, true, true },
            { "Value", PinType::Any, true }
        },
        {
            { "Label", PinType::String, "result" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildOutput(n); },
        nullptr,
        "Flow"
    });
}
