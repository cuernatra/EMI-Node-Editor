
#include "../nodeRegistry.h"
#include "../../compiler/nodeCompileHelpers.h"

namespace {
// Unified ArrayOperation node (backward compatible, does not replace existing nodes)
Node* CompileArrayOperationNode(GraphCompiler* compiler, const VisualNode& n)
{
    const std::string* opStr = FindField(n, "Operation");
    if (!opStr) {
        compiler->Error("ArrayOperation node missing Operation field");
        return nullptr;
    }

    const Pin* arrayPin = FindInputPin(n, "Array");
    if (!arrayPin) {
        compiler->Error("ArrayOperation node needs Array input");
        return nullptr;
    }
    Node* arrayExpr = BuildArrayInput(compiler, n, *arrayPin);
    if (!arrayExpr) return nullptr;

    if (*opStr == "Push" || *opStr == "PushFront" || *opStr == "PushUnique") {
        const Pin* valuePin = FindInputPin(n, "Value");
        if (!valuePin) {
            compiler->Error("ArrayOperation (" + *opStr + ") needs Value input");
            delete arrayExpr;
            return nullptr;
        }
        Node* valueExpr = compiler->BuildExpr(*valuePin);
        if (!valueExpr) {
            delete arrayExpr;
            return nullptr;
        }
            return MakeFunctionCallNode("Array." + *opStr, { arrayExpr, valueExpr });
    }
    else if (*opStr == "Clear") {
            return MakeFunctionCallNode("Array.Clear", { arrayExpr });
    }
    else if (*opStr == "Resize") {
        const Pin* sizePin = FindInputPin(n, "Size");
        if (!sizePin) {
            compiler->Error("ArrayOperation (Resize) needs Size input");
            delete arrayExpr;
            return nullptr;
        }
        Node* sizeExpr = compiler->BuildExpr(*sizePin);
        if (!sizeExpr) {
            delete arrayExpr;
            return nullptr;
        }
            return MakeFunctionCallNode("Array.Resize", { arrayExpr, sizeExpr });
    }
    else {
        compiler->Error("Unknown ArrayOperation: " + *opStr);
        delete arrayExpr;
        return nullptr;
    }
}

Node* CompileConstantNode(GraphCompiler* compiler, const VisualNode& n)
{
    const std::string* val = FindField(n, "Value");
    const std::string* type = FindField(n, "Type");

    const std::string value = val ? *val : "";
    const std::string typeName = type ? *type : "";

    if (typeName.empty())
    {
        compiler->Error("Constant node is missing required Type field");
        return nullptr;
    }

    if (typeName == "Boolean")
        return MakeBoolLiteral(value == "true" || value == "True" || value == "1");

    if (typeName == "Number")
    {
        try { return MakeNumberLiteral(std::stod(value)); }
        catch (...) { return MakeNumberLiteral(0.0); }
    }

    if (typeName == "String")
        return MakeStringLiteral(value);

    if (typeName == "Array")
        return compiler->BuildArrayLiteralNode(value);

    compiler->Error("Invalid Constant Type: " + typeName);
    return nullptr;
}

Node* CompileVariableNode(GraphCompiler* compiler, const VisualNode& n)
{
    const std::string* nameStr = FindField(n, "Name");
    const std::string* typeStr = FindField(n, "Type");
    const std::string* defaultStr = FindField(n, "Default");
    const std::string* variantStr = FindField(n, "Variant");

    const std::string varName = nameStr ? *nameStr : "__unnamed";

    if (variantStr && *variantStr == "Get")
        return MakeIdNode(varName);

    const PinType defaultType = VariableTypeFromString(typeStr ? *typeStr : "Number");
    const std::string defValue = defaultStr ? *defaultStr : "0.0";

    const Pin* setInput = FindInputPin(n, "Default");
    if (!setInput)
    {
        compiler->Error("Variable Set node is missing required Default input pin");
        return nullptr;
    }

    if (const PinSource* src = compiler->Resolve(*setInput))
    {
        Node* assign = MakeNode(Token::Assign);
        Node* rhs = compiler->BuildNode(*src->node, src->pinIdx);
        if (compiler->HasError) { delete assign; return nullptr; }
        assign->children.push_back(MakeIdNode(varName));
        assign->children.push_back(rhs);
        return assign;
    }

    Node* rhs = nullptr;
    switch (defaultType)
    {
        case PinType::Boolean:
            rhs = MakeBoolLiteral(defValue == "true" || defValue == "True" || defValue == "1");
            break;
        case PinType::String:
            rhs = MakeStringLiteral(defValue);
            break;
        case PinType::Array:
            rhs = compiler->BuildArrayLiteralNode(defValue);
            break;
        case PinType::Number:
            try { rhs = MakeNumberLiteral(std::stod(defValue)); }
            catch (...) { rhs = MakeNumberLiteral(0.0); }
            break;
        default:
            rhs = MakeNode(Token::Null);
            break;
    }

    Node* assign = MakeNode(Token::Assign);
    assign->children.push_back(MakeIdNode(varName));
    assign->children.push_back(rhs);
    return assign;
}

void CompilePreludeVariableNode(GraphCompiler* compiler, const VisualNode& n, Node*, Node* graphBodyScope)
{
    if (!compiler || !graphBodyScope)
        return;

    const std::string* variant = FindField(n, "Variant");
    const bool isGet = (variant && *variant == "Get");
    if (isGet)
        return;

    const std::string* nameStr = FindField(n, "Name");
    const std::string varName = (nameStr && !nameStr->empty()) ? *nameStr : "__unnamed";
    if (!compiler->TryDeclareGraphVariable(varName))
        return;

    const std::string* typeStr = FindField(n, "Type");
    const std::string typeName = typeStr ? *typeStr : "Number";

    Token typeToken = Token::TypeNumber;
    if (typeName == "Boolean")
        typeToken = Token::TypeBoolean;
    else if (typeName == "String")
        typeToken = Token::TypeString;
    else if (typeName == "Array")
        typeToken = Token::TypeArray;
    else if (typeName == "Any")
        typeToken = Token::AnyType;

    Node* decl = MakeNode(Token::VarDeclare);
    decl->data = varName;
    decl->children.push_back(MakeNode(typeToken));
    graphBodyScope->children.push_back(decl);
}

bool DeserializeVariableNode(VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds)
{
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
}

Node* CompileArrayGetNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* arrayPin = FindInputPin(n, "Array");
    const Pin* indexPin = FindInputPin(n, "Index");
    if (!arrayPin || !indexPin)
    {
        compiler->Error("Array Get node needs Array and Index inputs");
        return nullptr;
    }

    Node* arrayExpr = BuildArrayInput(compiler, n, *arrayPin);
    Node* indexExpr = BuildNumberOperand(compiler, n, *indexPin);
        return MakeIndexerNode(arrayExpr, indexExpr);
}

Node* CompileArrayAddNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* arrayPin = FindInputPin(n, "Array");
    const Pin* indexPin = FindInputPin(n, "Index");
    const Pin* valuePin = FindInputPin(n, "Value");
    if (!arrayPin || !indexPin || !valuePin)
    {
        compiler->Error("Array Add node needs Array, Index and Value inputs");
        return nullptr;
    }

    Node* arrayExpr = BuildArrayInput(compiler, n, *arrayPin);
    Node* indexExpr = BuildNumberOperand(compiler, n, *indexPin);
    Node* valueExpr = nullptr;
    if (compiler->Resolve(*valuePin))
    {
        valueExpr = compiler->BuildExpr(*valuePin);
    }
    else
    {
        const std::string* typeText  = FindField(n, "Add Type");
        const std::string* valueText = FindField(n, "Add Value");
        const std::string  typeName  = typeText  ? *typeText  : "Number";
        const std::string  valueName = valueText ? *valueText : "0.0";
        if (typeName == "Boolean")
            valueExpr = MakeBoolLiteral(valueName == "true" || valueName == "True" || valueName == "1");
        else if (typeName == "String")
            valueExpr = MakeStringLiteral(valueName);
        else if (typeName == "Array")
            valueExpr = compiler->BuildArrayLiteralNode(valueName);
        else
        {
            try { valueExpr = MakeNumberLiteral(std::stod(valueName)); }
            catch (...) { valueExpr = MakeNumberLiteral(0.0); }
        }
    }

    if (!arrayExpr || !indexExpr || !valueExpr)
    {
        delete arrayExpr;
        delete indexExpr;
        delete valueExpr;
        return nullptr;
    }

        return MakeFunctionCallNode("Array.Insert", { arrayExpr, indexExpr, valueExpr });
}

Node* CompileArrayReplaceNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* arrayPin = FindInputPin(n, "Array");
    const Pin* indexPin = FindInputPin(n, "Index");
    const Pin* valuePin = FindInputPin(n, "Value");
    if (!arrayPin || !indexPin || !valuePin)
    {
        compiler->Error("Array Replace node needs Array, Index and Value inputs");
        return nullptr;
    }

    Node* arrayExpr = BuildArrayInput(compiler, n, *arrayPin);
    Node* indexExpr = BuildNumberOperand(compiler, n, *indexPin);
    Node* valueExpr = nullptr;
    if (compiler->Resolve(*valuePin))
    {
        valueExpr = compiler->BuildExpr(*valuePin);
    }
    else
    {
        const std::string* typeText  = FindField(n, "Replace Type");
        const std::string* valueText = FindField(n, "Replace Value");
        const std::string  typeName  = typeText  ? *typeText  : "Number";
        const std::string  valueName = valueText ? *valueText : "0.0";
        if (typeName == "Boolean")
            valueExpr = MakeBoolLiteral(valueName == "true" || valueName == "True" || valueName == "1");
        else if (typeName == "String")
            valueExpr = MakeStringLiteral(valueName);
        else if (typeName == "Array")
            valueExpr = compiler->BuildArrayLiteralNode(valueName);
        else
        {
            try { valueExpr = MakeNumberLiteral(std::stod(valueName)); }
            catch (...) { valueExpr = MakeNumberLiteral(0.0); }
        }
    }

    if (!arrayExpr || !indexExpr || !valueExpr)
    {
        delete arrayExpr;
        delete indexExpr;
        delete valueExpr;
        return nullptr;
    }

    Node* scope = new Node();
    scope->type = Token::Scope;

    Node* removeCall = new Node();
    removeCall->type = Token::FunctionCall;
    removeCall->children.push_back(MakeIdNode("Array.RemoveIndex"));
    Node* removeParams = new Node();
    removeParams->type = Token::CallParams;
    removeParams->children.push_back(MakeIdNode("Array"));
    removeParams->children.push_back(MakeIdNode("Index"));
    removeCall->children.push_back(removeParams);

    Node* insertCall = new Node();
    insertCall->type = Token::FunctionCall;
    insertCall->children.push_back(MakeIdNode("Array.Insert"));
    Node* insertParams = new Node();
    insertParams->type = Token::CallParams;
    insertParams->children.push_back(MakeIdNode("Array"));
    insertParams->children.push_back(MakeIdNode("Index"));
    insertParams->children.push_back(MakeIdNode("Value"));
    insertCall->children.push_back(insertParams);

    Node* arrayDecl = new Node();
    arrayDecl->type = Token::VarDeclare;
    arrayDecl->data = "Array";
    arrayDecl->children.push_back(MakeNode(Token::TypeArray));
    arrayDecl->children.push_back(arrayExpr);

    Node* indexDecl = new Node();
    indexDecl->type = Token::VarDeclare;
    indexDecl->data = "Index";
    indexDecl->children.push_back(MakeNode(Token::TypeNumber));
    indexDecl->children.push_back(indexExpr);

    Node* valueDecl = new Node();
    valueDecl->type = Token::VarDeclare;
    valueDecl->data = "Value";
    valueDecl->children.push_back(MakeNode(Token::AnyType));
    valueDecl->children.push_back(valueExpr);

    scope->children.push_back(arrayDecl);
    scope->children.push_back(indexDecl);
    scope->children.push_back(valueDecl);
    scope->children.push_back(removeCall);
    scope->children.push_back(insertCall);
    return scope;
}

Node* CompileArrayRemoveNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* arrayPin = FindInputPin(n, "Array");
    const Pin* indexPin = FindInputPin(n, "Index");
    if (!arrayPin || !indexPin)
    {
        compiler->Error("Array Remove node needs Array and Index inputs");
        return nullptr;
    }

    Node* arrayExpr = BuildArrayInput(compiler, n, *arrayPin);
    Node* indexExpr = BuildNumberOperand(compiler, n, *indexPin);
    if (!arrayExpr || !indexExpr)
    {
        delete arrayExpr;
        delete indexExpr;
        return nullptr;
    }

        return MakeFunctionCallNode("Array.RemoveIndex", { arrayExpr, indexExpr });
}

Node* CompileArrayLengthNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* arrayPin = FindInputPin(n, "Array");
    if (!arrayPin)
    {
        compiler->Error("Array Length node needs Array input");
        return nullptr;
    }

    Node* arrayExpr = BuildArrayInput(compiler, n, *arrayPin);
    if (!arrayExpr)
        return nullptr;

        return MakeFunctionCallNode("Array.Size", { arrayExpr });
}


Node* CompileOutputNode(GraphCompiler* compiler, const VisualNode& n)
{
    Node* scope = new Node();
    scope->type = Token::Scope;

    const std::string* label = FindField(n, "Label");
    const std::string text = label ? *label : "result";

    scope->children.push_back(MakeFunctionCallNode(
        "println",
        { MakeStringLiteral("[Debug Print] " + text) }
    ));

    const Pin* valuePin = FindInputPin(n, "Value");
    if (valuePin)
    {
        Node* valueExpr = nullptr;
        if (compiler->Resolve(*valuePin))
        {
            valueExpr = compiler->BuildExpr(*valuePin);
        }
        else
        {
            // If unwired and fed by a ForEach, automatically use the ForEach element.
            const Pin* flowIn = FindInputPin(n, "In");
            if (!flowIn)
                flowIn = FindInputPin(n, "Flow");

            if (flowIn)
            {
                const FlowTarget* flowSrc = compiler->ResolveFlow(*flowIn);
                if (flowSrc && flowSrc->node && flowSrc->node->nodeType == NodeType::ForEach)
                {
                    int elementOutIdx = -1;
                    for (int i = 0; i < static_cast<int>(flowSrc->node->outPins.size()); ++i)
                    {
                        const Pin& outPin = flowSrc->node->outPins[static_cast<size_t>(i)];
                        if (outPin.type != PinType::Flow && outPin.name == "Element")
                        {
                            elementOutIdx = i;
                            break;
                        }
                    }
                    if (elementOutIdx >= 0)
                        valueExpr = compiler->BuildNode(*flowSrc->node, elementOutIdx);
                }
            }

            if (!valueExpr)
                valueExpr = compiler->BuildExpr(*valuePin);
        }

        if (!valueExpr)
        {
            delete scope;
            return nullptr;
        }

        scope->children.push_back(MakeFunctionCallNode("println", { valueExpr }));
    }

    return scope;
}

Node* CompilePreviewPickRectNode(GraphCompiler*, const VisualNode&)
{
    // Preview-only event node: no runtime AST emission required,
    // but return a valid no-op scope so registry/compiler validation passes.
    return MakeNode(Token::Scope);
}

Node* CompileArrayReverseNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* arrayPin = FindInputPin(n, "Array");
    if (!arrayPin)
    {
        compiler->Error("Array Reverse node needs Array input");
        return nullptr;
    }

    Node* arrayExpr = BuildArrayInput(compiler, n, *arrayPin);
    return MakeFunctionCallNode("Array.Reverse", { arrayExpr });
}

Node* CompileArrayContainsNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* arrayPin = FindInputPin(n, "Array");
    const Pin* valuePin = FindInputPin(n, "Value");
    if (!arrayPin || !valuePin)
    {
        compiler->Error("Array Contains node needs Array and Value inputs");
        return nullptr;
    }

    Node* arrayExpr = BuildArrayInput(compiler, n, *arrayPin);
    Node* valueExpr = nullptr;
    if (compiler->Resolve(*valuePin))
        valueExpr = compiler->BuildExpr(*valuePin);
    else
        valueExpr = MakeNumberLiteral(0.0);

    return MakeFunctionCallNode("Array.Contains", { arrayExpr, valueExpr });
}
}

void NodeRegistry::RegisterDataNodes()
{
    // Use named initializers so it's obvious what NodeDescriptor fields exist.

    // Unified ArrayOperation node (backward compatible, does not replace existing nodes)
    Register(NodeDescriptor{
        .type = NodeType::ArrayOperation,
        .label = "Array Operation",
        .pins = {
            { "In", PinType::Flow, true },
            { "Array", PinType::Array, true },
            { "Value", PinType::Any, false }, // Only used for Push/PushFront/PushUnique
            { "Size", PinType::Number, false }, // Only used for Resize
            { "Out", PinType::Flow, false }
        },
        .fields = {
            { "Operation", PinType::String, "Push", { "Push", "PushFront", "PushUnique", "Clear", "Resize" } }
        },
        .compile = CompileArrayOperationNode,
        .deserialize = nullptr,
        .category = "Data",
        .paletteVariants = {},
        .saveToken = "ArrayOperation",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Array
    });

    Register(NodeDescriptor{
        .type = NodeType::Constant,
        .label = "Constant",
        .pins = {
            { "Value", PinType::Any, false }
        },
        .fields = {
            { "Type", PinType::String, "Number", { "Number", "Boolean", "String", "Array" } },
            { "Value", PinType::Any, "0.0" }
        },
        .compile = CompileConstantNode,
        .deserialize = nullptr,
        .category = "Data",
        .paletteVariants = {},
        .saveToken = "Constant",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Constant
    });

    Register(NodeDescriptor{
        .type = NodeType::Variable,
        .label = "Set Variable",
        .pins = {
            { "In", PinType::Flow, true },
            { "Default", PinType::Any, true },
            { "Out", PinType::Flow, false }
        },
        .fields = {
            { "Variant", PinType::String, "Set", { "Set", "Get" } },
            { "Name", PinType::String, "myVar" },
            { "Type", PinType::String, "Number", { "Number", "Boolean", "String", "Array" } },
            { "Default", PinType::String, "0.0" }
        },
        .compile = CompileVariableNode,
        .compilePrelude = CompilePreludeVariableNode,
        .deserialize = DeserializeVariableNode,
        .category = "Data",
        .paletteVariants = {
            { "Set Variable", "Variable:Set" },
            { "Get Variable", "Variable:Get" }
        },
        .saveToken = "Variable",
        .deferredInputPins = { "Default" },
        .renderStyle = NodeRenderStyle::Variable
    });

    Register(NodeDescriptor{
        .type = NodeType::ArrayGetAt,
        .label = "Array Get",
        .pins = {
            { "Array", PinType::Array, true },
            { "Index", PinType::Number, true },
            { "Value", PinType::Any, false }
        },
        .fields = {
            { "Array", PinType::Array, "[]" },
            { "Index", PinType::Number, "0" }
        },
        .compile = CompileArrayGetNode,
        .deserialize = nullptr,
        .category = "Data",
        .paletteVariants = {},
        .saveToken = "ArrayGet",
        .deferredInputPins = { "Index" },
        .renderStyle = NodeRenderStyle::Array
    });

    Register(NodeDescriptor{
        .type = NodeType::ArrayAddAt,
        .label = "Array Add",
        .pins = {
            { "In", PinType::Flow, true },
            { "Array", PinType::Array, true },
            { "Index", PinType::Number, true },
            { "Value", PinType::Any, true },
            { "Out", PinType::Flow, false }
        },
        .fields = {
            { "Array", PinType::Array, "[]" },
            { "Index", PinType::Number, "0" },
            { "Add Type", PinType::String, "Number", { "Number", "Boolean", "String", "Array" } },
            { "Add Value", PinType::Any, "0.0" }
        },
        .compile = CompileArrayAddNode,
        .deserialize = nullptr,
        .category = "Data",
        .paletteVariants = {},
        .saveToken = "ArrayAdd",
        .deferredInputPins = { "Index" },
        .renderStyle = NodeRenderStyle::Array
    });

    Register(NodeDescriptor{
        .type = NodeType::ArrayReplaceAt,
        .label = "Array Replace",
        .pins = {
            { "In", PinType::Flow, true },
            { "Array", PinType::Array, true },
            { "Index", PinType::Number, true },
            { "Value", PinType::Any, true },
            { "Out", PinType::Flow, false }
        },
        .fields = {
            { "Array", PinType::Array, "[]" },
            { "Index", PinType::Number, "0" },
            { "Replace Type", PinType::String, "Number", { "Number", "Boolean", "String", "Array" } },
            { "Replace Value", PinType::Any, "0.0" }
        },
        .compile = CompileArrayReplaceNode,
        .deserialize = nullptr,
        .category = "Data",
        .paletteVariants = {},
        .saveToken = "ArrayReplace",
        .deferredInputPins = { "Index" },
        .renderStyle = NodeRenderStyle::Array
    });

    Register(NodeDescriptor{
        .type = NodeType::ArrayRemoveAt,
        .label = "Array Remove",
        .pins = {
            { "In", PinType::Flow, true },
            { "Array", PinType::Array, true },
            { "Index", PinType::Number, true },
            { "Out", PinType::Flow, false }
        },
        .fields = {
            { "Index", PinType::Number, "0" }
        },
        .compile = CompileArrayRemoveNode,
        .deserialize = nullptr,
        .category = "Data",
        .paletteVariants = {},
        .saveToken = "ArrayRemove",
        .deferredInputPins = { "Index" },
        .renderStyle = NodeRenderStyle::Array
    });

    Register(NodeDescriptor{
        .type = NodeType::ArrayLength,
        .label = "Array Length",
        .pins = {
            { "Array", PinType::Array, true },
            { "Length", PinType::Number, false }
        },
        .fields = {
            { "Array", PinType::Array, "[]" }
        },
        .compile = CompileArrayLengthNode,
        .deserialize = nullptr,
        .category = "Data",
        .paletteVariants = {},
        .saveToken = "ArrayLength",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Array
    });

    Register(NodeDescriptor{
        .type = NodeType::PreviewPickRect,
        .label = "Rect Click",
        .pins = {
            { "Out", PinType::Flow, false },
            { "X", PinType::Number, false },
            { "Y", PinType::Number, false }
        },
        .fields = {},
        .compile = CompilePreviewPickRectNode,
        .deserialize = nullptr,
        .category = "Data",
        .paletteVariants = {},
        .saveToken = "PreviewPickRect",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Default
    });

    Register(NodeDescriptor{
        .type = NodeType::ArrayReverse,
        .label = "Array Reverse",
        .pins = {
            { "In", PinType::Flow, true },
            { "Array", PinType::Array, true },
            { "Out", PinType::Flow, false }
        },
        .fields = {},
        .compile = CompileArrayReverseNode,
        .deserialize = nullptr,
        .category = "Data",
        .paletteVariants = {},
        .saveToken = "ArrayReverse",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Array
    });

    Register(NodeDescriptor{
        .type = NodeType::ArrayContains,
        .label = "Array Contains",
        .pins = {
            { "Array", PinType::Array, true },
            { "Value", PinType::Any, true },
            { "Result", PinType::Boolean, false }
        },
        .fields = {
            { "Array", PinType::Array, "[]" }
        },
        .compile = CompileArrayContainsNode,
        .deserialize = nullptr,
        .category = "Data",
        .paletteVariants = {},
        .saveToken = "ArrayContains",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Array
    });

    Register(NodeDescriptor{
        .type = NodeType::Output,
        .label = "Debug Print",
        .pins = {
            { "In", PinType::Flow, true, true },
            { "Value", PinType::Any, true }
        },
        .fields = {
            { "Label", PinType::String, "result" }
        },
        .compile = CompileOutputNode,
        .deserialize = nullptr,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "Output",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Default
    });
}
