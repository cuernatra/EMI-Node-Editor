#include "../nodeRegistry.h"
#include "../../compiler/nodeCompileHelpers.h"

namespace
{
// ─── Schema helpers ───────────────────────────────────────────────────────────

struct SchemaField
{
    std::string name;
    PinType     type;
};

// Parse the "Schema Fields" text (a JSON array of "name:Type" strings)
// into a flat list of SchemaField entries.
static std::vector<SchemaField> ParseSchemaFields(const VisualNode& n)
{
    const std::string* schemaText = FindField(n, "Schema Fields");
    std::string inner = schemaText ? TrimCopy(*schemaText) : "";
    if (inner.size() >= 2 && inner.front() == '[' && inner.back() == ']')
        inner = inner.substr(1, inner.size() - 2);

    std::vector<SchemaField> result;
    for (std::string raw : SplitArrayItemsTopLevel(inner))
    {
        raw = TrimCopy(raw);
        if (raw.size() >= 2 &&
            ((raw.front() == '"' && raw.back() == '"') ||
             (raw.front() == '\'' && raw.back() == '\'')))
            raw = raw.substr(1, raw.size() - 2);

        const size_t sep = raw.find(':');
        SchemaField f;
        f.name = (sep == std::string::npos) ? raw              : TrimCopy(raw.substr(0, sep));
        f.type = (sep == std::string::npos) ? PinType::Any     : VariableTypeFromString(TrimCopy(raw.substr(sep + 1)));
        result.push_back(std::move(f));
    }
    return result;
}

// Build a typed literal value node from a raw string, falling back to a
// type-appropriate zero when the string is absent or unparseable.
static Node* BuildFieldLiteral(PinType type, const std::string* raw, GraphCompiler* compiler)
{
    if (!raw)
    {
        switch (type)
        {
            case PinType::Number:  return MakeNumberLiteral(0.0);
            case PinType::Boolean: return MakeBoolLiteral(false);
            case PinType::String:  return MakeStringLiteral("");
            case PinType::Array:
            case PinType::Struct:  return MakeNode(Token::Array);
            default:               return MakeNode(Token::Null);
        }
    }

    switch (type)
    {
        case PinType::Number:
        {
            double v = 0.0;
            if (TryParseDoubleExact(*raw, v)) return MakeNumberLiteral(v);
            return MakeNumberLiteral(0.0);
        }
        case PinType::Boolean:
            return MakeBoolLiteral(*raw == "true" || *raw == "True" || *raw == "1");
        case PinType::String:
            return MakeStringLiteral(*raw);
        case PinType::Array:
        case PinType::Struct:
            return compiler->BuildArrayLiteralNode(raw->empty() ? "[]" : *raw);
        default:
            return MakeStringLiteral(*raw);
    }
}

static std::string GetInstanceName(const VisualNode& n)
{
    const std::string* f = FindField(n, "Instance Name");
    return (f && !f->empty()) ? *f : std::string("structItem");
}

// ─── StructCreate ─────────────────────────────────────────────────────────────

// Custom load callback: In + Out are the static flow pins; every pin in
// between is a dynamic field-input pin whose name/type the refresh pass
// will fill in from the schema.  Placeholder names ("__pin0__", …) let
// the refresh match by position and reuse the saved pin IDs (preserving
// existing links).
bool DeserializeStructCreateNode(VisualNode& n, const NodeDescriptor& desc,
                                 const std::vector<int>& pinIds)
{
    if (pinIds.size() < 2)
        return false;

    for (const FieldDescriptor& fd : desc.fields)
        n.fields.push_back(MakeNodeField(fd));

    n.inPins.push_back(MakePin(
        static_cast<uint32_t>(pinIds[0]),
        n.id, desc.type, "In", PinType::Flow, true));

    for (size_t i = 1; i + 1 < pinIds.size(); ++i)
    {
        const std::string placeholder = "__pin" + std::to_string(i - 1) + "__";
        n.inPins.push_back(MakePin(
            static_cast<uint32_t>(pinIds[i]),
            n.id, desc.type, placeholder, PinType::Any, true));
    }

    n.outPins.push_back(MakePin(
        static_cast<uint32_t>(pinIds.back()),
        n.id, desc.type, "Out", PinType::Flow, false));

    return true;
}

// Re-initialise the struct instance variable with current field values.
// Wired input pins take priority over the inspector field defaults.
Node* CompileStructCreateNode(GraphCompiler* compiler, const VisualNode& n)
{
    const std::vector<SchemaField> schema = ParseSchemaFields(n);

    Node* arr = MakeNode(Token::Array);
    for (const SchemaField& f : schema)
    {
        if (const Pin* pin = FindInputPin(n, f.name.c_str()))
        {
            if (const PinSource* src = compiler->Resolve(*pin))
            {
                arr->children.push_back(compiler->BuildNode(*src->node, src->pinIdx));
                if (compiler->HasError) { delete arr; return nullptr; }
                continue;
            }
        }
        arr->children.push_back(BuildFieldLiteral(f.type, FindField(n, f.name.c_str()), compiler));
        if (compiler->HasError) { delete arr; return nullptr; }
    }

    Node* assign = MakeNode(Token::Assign);
    assign->children.push_back(MakeIdNode(GetInstanceName(n)));
    assign->children.push_back(arr);
    return assign;
}

// Declare and zero-initialise the struct instance before the flow chain runs.
void CompileStructCreatePrelude(GraphCompiler* compiler, const VisualNode& n,
                                Node*, Node* graphBodyScope)
{
    if (!compiler || !graphBodyScope)
        return;

    if (!compiler->TryDeclareGraphVariable(GetInstanceName(n)))
        return;

    const std::vector<SchemaField> schema = ParseSchemaFields(n);

    Node* initArray = MakeNode(Token::Array);
    for (const SchemaField& f : schema)
    {
        if (const Pin* pin = FindInputPin(n, f.name.c_str()))
        {
            if (const PinSource* src = compiler->Resolve(*pin))
            {
                initArray->children.push_back(compiler->BuildNode(*src->node, src->pinIdx));
                if (compiler->HasError) { delete initArray; return; }
                continue;
            }
        }
        initArray->children.push_back(BuildFieldLiteral(f.type, FindField(n, f.name.c_str()), compiler));
    }

    Node* decl = MakeNode(Token::VarDeclare);
    decl->data = GetInstanceName(n);
    decl->children.push_back(MakeNode(Token::TypeArray));
    decl->children.push_back(initArray);
    graphBodyScope->children.push_back(decl);
}

// ─── StructRead ───────────────────────────────────────────────────────────────

// Returns instanceName[fieldIndex] where fieldIndex is looked up by name.
Node* CompileStructReadNode(GraphCompiler* /*compiler*/, const VisualNode& n)
{
    const std::vector<SchemaField> schema = ParseSchemaFields(n);

    const std::string* fieldNameField = FindField(n, "Field Name");
    const std::string  fieldName = (fieldNameField && !fieldNameField->empty())
                                   ? *fieldNameField : std::string();

    int fieldIndex = 0;
    bool found = false;
    for (int i = 0; i < static_cast<int>(schema.size()); ++i)
    {
        if (schema[static_cast<size_t>(i)].name == fieldName)
        {
            fieldIndex = i;
            found = true;
            break;
        }
    }

    if (!found && schema.empty())
        return MakeNode(Token::Null);

    return MakeIndexerNode(
        MakeIdNode(GetInstanceName(n)),
        MakeNumberLiteral(static_cast<double>(fieldIndex))
    );
}

// ─── StructWrite ──────────────────────────────────────────────────────────────

// Emits: instanceName[fieldIndex] = value
Node* CompileStructWriteNode(GraphCompiler* compiler, const VisualNode& n)
{
    const std::vector<SchemaField> schema = ParseSchemaFields(n);
    if (schema.empty())
        return MakeNode(Token::Scope);

    const std::string* fieldNameField = FindField(n, "Field Name");
    const std::string  fieldName = (fieldNameField && !fieldNameField->empty())
                                   ? *fieldNameField : std::string();

    int      fieldIndex = 0;
    PinType  fieldType  = schema[0].type;
    for (int i = 0; i < static_cast<int>(schema.size()); ++i)
    {
        if (schema[static_cast<size_t>(i)].name == fieldName)
        {
            fieldIndex = i;
            fieldType  = schema[static_cast<size_t>(i)].type;
            break;
        }
    }

    const Pin* valuePin = FindInputPin(n, "Value");
    if (!valuePin)
    {
        compiler->Error("Struct Write needs Value input");
        return nullptr;
    }

    Node* rhs = nullptr;
    if (const PinSource* src = compiler->Resolve(*valuePin))
    {
        rhs = compiler->BuildNode(*src->node, src->pinIdx);
        if (compiler->HasError) { delete rhs; return nullptr; }
    }
    else
    {
        rhs = BuildFieldLiteral(fieldType, FindField(n, "Value"), compiler);
    }

    Node* assign = MakeNode(Token::Assign);
    assign->children.push_back(MakeIndexerNode(
        MakeIdNode(GetInstanceName(n)),
        MakeNumberLiteral(static_cast<double>(fieldIndex))
    ));
    assign->children.push_back(rhs);
    return assign;
}

// StructDefine — schema editor only, no runtime execution.
// Returns a no-op scope so registry/compiler validation passes.
Node* CompileStructDefineNode(GraphCompiler*, const VisualNode&)
{
    return MakeNode(Token::Scope);
}

} // namespace

void NodeRegistry::RegisterStructNodes()
{
    // StructDefine — schema editor only, no runtime execution.
    Register(NodeDescriptor{
        .type      = NodeType::StructDefine,
        .label     = "Struct Define",
        .pins      = {},
        .fields    = {
            { "Struct Name", PinType::String, "test"                           },
            { "Fields",      PinType::Array,  "[\"id:Number\", \"type:String\"]" }
        },
        .compile   = CompileStructDefineNode,
        .category  = "Structs",
        .saveToken = "StructDefine",
        .renderStyle = NodeRenderStyle::StructDefine,
    });

    // StructCreate — flow node that declares + initialises the struct variable.
    // Static pins: In / Out flow.  Dynamic field-input pins are managed by
    // RefreshStructNodeLayouts; their IDs survive save/load via DeserializeStructCreateNode.
    Register(NodeDescriptor{
        .type  = NodeType::StructCreate,
        .label = "Struct Create",
        .pins  = {
            { "In",  PinType::Flow, true  },
            { "Out", PinType::Flow, false },
        },
        .fields = {
            { "Struct Name",   PinType::String, "test"       },
            { "Instance Name", PinType::String, "structItem" },
            { "Schema Fields", PinType::Array,  "[]"         },
        },
        .compile        = CompileStructCreateNode,
        .compilePrelude = CompileStructCreatePrelude,
        .deserialize    = DeserializeStructCreateNode,
        .category       = "Structs",
        .saveToken      = "StructCreate",
        .renderStyle    = NodeRenderStyle::StructCreate,
    });

    // StructRead — pure value node, reads one named field from the instance.
    Register(NodeDescriptor{
        .type  = NodeType::StructRead,
        .label = "Struct Read",
        .pins  = {
            { "Value", PinType::Any, false },
        },
        .fields = {
            { "Struct Name",   PinType::String, "test"       },
            { "Instance Name", PinType::String, "structItem" },
            { "Field Name",    PinType::String, "field"      },
            { "Schema Fields", PinType::Array,  "[]"         },
        },
        .compile   = CompileStructReadNode,
        .category  = "Structs",
        .saveToken = "StructRead",
        .renderStyle = NodeRenderStyle::StructRead,
    });

    // StructWrite — flow node, writes one named field into the instance.
    Register(NodeDescriptor{
        .type  = NodeType::StructWrite,
        .label = "Struct Write",
        .pins  = {
            { "In",    PinType::Flow, true  },
            { "Value", PinType::Any,  true  },
            { "Out",   PinType::Flow, false },
        },
        .fields = {
            { "Struct Name",   PinType::String, "test"       },
            { "Instance Name", PinType::String, "structItem" },
            { "Field Name",    PinType::String, "field"      },
            { "Value",         PinType::Any,    "0.0"        },
            { "Schema Fields", PinType::Array,  "[]"         },
        },
        .compile           = CompileStructWriteNode,
        .category          = "Structs",
        .saveToken         = "StructWrite",
        .deferredInputPins = { "Value" },
        .renderStyle       = NodeRenderStyle::StructWrite,
    });
}
