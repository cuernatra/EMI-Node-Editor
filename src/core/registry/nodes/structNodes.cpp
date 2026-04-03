#include "../nodeRegistry.h"
#include "nodeCompileHelpers.h"

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
        [](GraphCompiler* compiler, const VisualNode& n) -> Node*
        {
            const std::string* fields = FindField(n, "Fields");
            return compiler->BuildArrayLiteralNode(fields ? *fields : "[]");
        },
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
        [](GraphCompiler* compiler, const VisualNode& n) { return BuildStructCreateNode(compiler, n); },
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
