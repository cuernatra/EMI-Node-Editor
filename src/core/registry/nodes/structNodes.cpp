#include "../nodeRegistry.h"
#include "../../compiler/nodeCompileHelpers.h"

namespace
{
Node* CompileStructDefineNode(GraphCompiler* compiler, const VisualNode& n)
{
    const std::string* fields = FindField(n, "Fields");
    return compiler->BuildArrayLiteralNode(fields ? *fields : "[]");
}

Node* CompileStructCreateNode(GraphCompiler* compiler, const VisualNode& n)
{
    const std::string* schemaText = FindField(n, "Schema Fields");
    std::string inner = schemaText ? TrimCopy(*schemaText) : "";
    if (inner.size() >= 2 && inner.front() == '[' && inner.back() == ']')
        inner = inner.substr(1, inner.size() - 2);
    const std::vector<std::string> defs = SplitArrayItemsTopLevel(inner);

    auto parseBool = [](const std::string& s) -> bool {
        return s == "true" || s == "True" || s == "1";
    };

    auto defaultForType = [&](PinType t) -> Node*
    {
        switch (t)
        {
            case PinType::Number:  return MakeNumberLiteral(0.0);
            case PinType::Boolean: return MakeBoolLiteral(false);
            case PinType::String:  return MakeStringLiteral("");
            case PinType::Array:   return compiler->BuildArrayLiteralNode("[]");
            default:               return MakeNode(Token::Null);
        }
    };

    auto buildTypedLiteral = [&](PinType t, const std::string* raw) -> Node*
    {
        if (!raw) return defaultForType(t);
        switch (t)
        {
            case PinType::Number:
            {
                double v = 0.0;
                if (TryParseDoubleExact(*raw, v)) return MakeNumberLiteral(v);
                return MakeNumberLiteral(0.0);
            }
            case PinType::Boolean: return MakeBoolLiteral(parseBool(*raw));
            case PinType::String:  return MakeStringLiteral(*raw);
            case PinType::Array:   return compiler->BuildArrayLiteralNode(*raw);
            default:               return MakeStringLiteral(*raw);
        }
    };

    Node* arr = MakeNode(Token::Array);
    for (std::string raw : defs)
    {
        raw = TrimCopy(raw);
        if (raw.size() >= 2 &&
            ((raw.front() == '"' && raw.back() == '"') ||
             (raw.front() == '\'' && raw.back() == '\'')))
            raw = raw.substr(1, raw.size() - 2);

        const size_t sep = raw.find(':');
        const std::string fieldName = (sep == std::string::npos) ? raw : TrimCopy(raw.substr(0, sep));
        const std::string typeName  = (sep == std::string::npos) ? "Any" : TrimCopy(raw.substr(sep + 1));
        const PinType fieldType = VariableTypeFromString(typeName);

        if (const Pin* pin = FindInputPin(n, fieldName.c_str()))
        {
            if (const PinSource* src = compiler->Resolve(*pin))
            {
                arr->children.push_back(compiler->BuildNode(*src->node, src->pinIdx));
                if (compiler->HasError) { delete arr; return nullptr; }
                continue;
            }
        }

        const std::string* fieldValue = FindField(n, fieldName.c_str());
        arr->children.push_back(buildTypedLiteral(fieldType, fieldValue));
        if (compiler->HasError) { delete arr; return nullptr; }
    }
    return arr;
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
    Register(NodeDescriptor{
        .type = NodeType::StructDefine,
        .label = "Struct Define",
        .pins = {
            { "Schema", PinType::Array, false }
        },
        .fields = {
            { "Struct Name", PinType::String, "test" },
            { "Fields", PinType::Array, "[\"id:Number\", \"type:String\"]" }
        },
        .compile = CompileStructDefineNode,
        .deserialize = nullptr,
        .category = "Structs",
        .paletteVariants = {},
        .saveToken = "StructDefine",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::StructDefine
    });

    Register(NodeDescriptor{
        .type = NodeType::StructCreate,
        .label = "Struct Create",
        .pins = {
            { "Struct", PinType::String, true },
            { "Value", PinType::Any, true },
            { "Item", PinType::Array, false }
        },
        .fields = {
            { "Struct Name", PinType::String, "test" },
            { "Schema Fields", PinType::Array, "[]" }
        },
        .compile = CompileStructCreateNode,
        .deserialize = DeserializeStructCreateNode,
        .category = "Structs",
        .paletteVariants = {},
        .saveToken = "StructCreate",
        .deferredInputPins = { "*" },
        .renderStyle = NodeRenderStyle::StructCreate
    });
}
