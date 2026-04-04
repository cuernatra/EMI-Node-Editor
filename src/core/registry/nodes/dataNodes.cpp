#include "../nodeRegistry.h"
#include "nodeCompileHelpers.h"

namespace
{
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

    Node* arrayExpr = BuildArrayInput(compiler, n, *arrayPin, "Array");
    Node* indexExpr = BuildNumberInput(compiler, n, *indexPin, "Index");
    return compiler->EmitIndexer(arrayExpr, indexExpr);
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

    Node* arrayExpr = BuildArrayInput(compiler, n, *arrayPin, "Array");
    Node* indexExpr = BuildNumberInput(compiler, n, *indexPin, "Index");
    Node* valueExpr = nullptr;
    if (compiler->Resolve(*valuePin))
        valueExpr = compiler->BuildExpr(*valuePin);
    else
        valueExpr = BuildTypedFieldValue(compiler, n, "Add Type", "Add Value");

    if (!arrayExpr || !indexExpr || !valueExpr)
    {
        delete arrayExpr;
        delete indexExpr;
        delete valueExpr;
        return nullptr;
    }

    return compiler->EmitFunctionCall("Array.Insert", { arrayExpr, indexExpr, valueExpr });
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

    Node* arrayExpr = BuildArrayInput(compiler, n, *arrayPin, "Array");
    Node* indexExpr = BuildNumberInput(compiler, n, *indexPin, "Index");
    Node* valueExpr = nullptr;
    if (compiler->Resolve(*valuePin))
        valueExpr = compiler->BuildExpr(*valuePin);
    else
        valueExpr = BuildTypedFieldValue(compiler, n, "Replace Type", "Replace Value");

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

    Node* arrayExpr = BuildArrayInput(compiler, n, *arrayPin, "Array");
    Node* indexExpr = BuildNumberInput(compiler, n, *indexPin, "Index");
    if (!arrayExpr || !indexExpr)
    {
        delete arrayExpr;
        delete indexExpr;
        return nullptr;
    }

    return compiler->EmitFunctionCall("Array.RemoveIndex", { arrayExpr, indexExpr });
}

Node* CompileArrayLengthNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* arrayPin = FindInputPin(n, "Array");
    if (!arrayPin)
    {
        compiler->Error("Array Length node needs Array input");
        return nullptr;
    }

    Node* arrayExpr = BuildArrayInput(compiler, n, *arrayPin, "Array");
    if (!arrayExpr)
        return nullptr;

    return compiler->EmitFunctionCall("Array.Size", { arrayExpr });
}

Node* CompileOutputNode(GraphCompiler* compiler, const VisualNode& n)
{
    Node* scope = new Node();
    scope->type = Token::Scope;

    const std::string* label = FindField(n, "Label");
    const std::string text = label ? *label : "result";

    scope->children.push_back(compiler->EmitFunctionCall(
        "println",
        { ParseTextLiteralNode("[Debug Print] " + text) }
    ));

    const Pin* valuePin = FindInputPin(n, "Value");
    if (valuePin)
    {
        Node* valueExpr = BuildOutputValue(compiler, n, *valuePin);
        if (!valueExpr)
        {
            delete scope;
            return nullptr;
        }

        scope->children.push_back(compiler->EmitFunctionCall("println", { valueExpr }));
    }

    return scope;
}
}

void NodeRegistry::RegisterDataNodes()
{
    // Register(...) fields follow NodeDescriptor member order.
    Register({
        NodeType::Constant,
        "Constant",
        {
            { "Value", PinType::Any, false }
        },
        {
            { "Type", PinType::String, "Number", { "Number", "Boolean", "String", "Array" } },
            { "Value", PinType::Any, "0.0" }
        },
        CompileConstantNode,
        nullptr,
        "Data",
        {},
        "Constant",
        {},
        NodeRenderStyle::Constant
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
            { "Variant", PinType::String, "Set", { "Set", "Get" } },
            { "Name", PinType::String, "myVar" },
            { "Type", PinType::String, "Number", { "Number", "Boolean", "String", "Array" } },
            { "Default", PinType::String, "0.0" }
        },
        CompileVariableNode,
        DeserializeVariableNode,
        "Data",
        {
            { "Set Variable", "Variable:Set" },
            { "Get Variable", "Variable:Get" }
        },
        "Variable",
        { "Default" },
        NodeRenderStyle::Variable
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
        CompileArrayGetNode,
        nullptr,
        "Data",
        {},
        "ArrayGet",
        { "Index" },
        NodeRenderStyle::Array
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
            { "Add Type", PinType::String, "Number", { "Number", "Boolean", "String", "Array" } },
            { "Add Value", PinType::Any, "0.0" }
        },
        CompileArrayAddNode,
        nullptr,
        "Data",
        {},
        "ArrayAdd",
        { "Index" },
        NodeRenderStyle::Array
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
            { "Replace Type", PinType::String, "Number", { "Number", "Boolean", "String", "Array" } },
            { "Replace Value", PinType::Any, "0.0" }
        },
        CompileArrayReplaceNode,
        nullptr,
        "Data",
        {},
        "ArrayReplace",
        { "Index" },
        NodeRenderStyle::Array
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
        CompileArrayRemoveNode,
        nullptr,
        "Data",
        {},
        "ArrayRemove",
        { "Index" },
        NodeRenderStyle::Array
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
        CompileArrayLengthNode,
        nullptr,
        "Data",
        {},
        "ArrayLength",
        {},
        NodeRenderStyle::Array
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
        CompileOutputNode,
        nullptr,
        "Flow",
        {},
        "Output"
    });
}
