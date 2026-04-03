#include "../nodeRegistry.h"
#include "nodeCompileHelpers.h"

namespace
{
Node* CompileStructDefineNode(GraphCompiler* compiler, const VisualNode& n)
{
    const std::string* fields = FindField(n, "Fields");
    return compiler->BuildArrayLiteralNode(fields ? *fields : "[]");
}

Node* CompileStructCreateNode(GraphCompiler* compiler, const VisualNode& n)
{
    return BuildStructCreateNode(compiler, n);
}

bool DeserializeStructCreateNode(VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds)
{
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
        CompileStructDefineNode,
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
        CompileStructCreateNode,
        DeserializeStructCreateNode,
        "Structs"
    });
}
