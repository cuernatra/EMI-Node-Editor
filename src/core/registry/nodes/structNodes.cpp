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

void NodeRegistry::RegisterStructNodes()
{
    Register({
        NodeType::StructDefine,
        "Struct Define",
        {
            { "Schema", PinType::Array, false }
        },
        {
            { "Struct Name", PinType::String, "test" },
            { "Fields", PinType::Array, "[\"id:Number\", \"type:String\"]" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildStructDefine(n); },
        nullptr,
        "Structs"
    });

    Register({
        NodeType::StructCreate,
        "Struct Create",
        {
            { "Struct", PinType::String, true },
            { "Value", PinType::Any, true },
            { "Item", PinType::Array, false }
        },
        {
            { "Struct Name", PinType::String, "test" },
            { "Schema Fields", PinType::Array, "[]" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildStructCreate(n); },
        [](VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds) -> bool {
            if (pinIds.size() == desc.pins.size())
                return PopulateExactPinsAndFields(n, desc, pinIds);

            if (pinIds.size() < 2)
                return false;

            n.inPins.push_back(MakePin(
                static_cast<uint32_t>(pinIds[0]),
                n.id,
                desc.type,
                "Struct",
                PinType::String,
                true
            ));

            for (size_t i = 1; i + 1 < pinIds.size(); ++i)
            {
                n.inPins.push_back(MakePin(
                    static_cast<uint32_t>(pinIds[i]),
                    n.id,
                    desc.type,
                    "Field " + std::to_string(i),
                    PinType::Any,
                    true
                ));
            }

            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(pinIds.back()),
                n.id,
                desc.type,
                "Item",
                PinType::Array,
                false
            ));

            for (const FieldDescriptor& fd : desc.fields)
                n.fields.push_back(MakeNodeField(fd));

            return true;
        },
        "Structs"
    });
}
