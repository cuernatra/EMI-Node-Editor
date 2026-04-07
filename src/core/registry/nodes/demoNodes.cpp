#include "../nodeRegistry.h"
#include "../../compiler/nodeCompileHelpers.h"

namespace
{
Node* CompileNativeCallNode(GraphCompiler* compiler, const VisualNode& n)
{
    const std::string* nameStr = FindField(n, "Function");
    const std::string funcName = nameStr ? *nameStr : "";
    if (funcName.empty())
    {
        compiler->Error("Native Call requires Function field");
        return nullptr;
    }

    int argCount = 0;
    const std::string* argCountStr = FindField(n, "ArgCount");
    if (argCountStr)
    {
        try { argCount = std::clamp(std::stoi(*argCountStr), 0, 6); }
        catch (...) { argCount = 0; }
    }

    std::vector<Node*> args;
    args.reserve(static_cast<size_t>(argCount));

    for (int i = 0; i < argCount; ++i)
    {
        const std::string pinName = "Arg" + std::to_string(i);
        const Pin* pin = FindInputPin(n, pinName.c_str());
        if (!pin)
            continue;
        args.push_back(BuildNumberOperand(compiler, n, *pin));
        if (compiler->HasError)
            return nullptr;
    }

    // Always emit as an expression so the value can be used if desired
    return compiler->EmitFunctionCall(funcName, std::move(args));
}

Node* CompileNativeGetNode(GraphCompiler* compiler, const VisualNode& n)
{
    const std::string* nameStr = FindField(n, "Function");
    const std::string funcName = nameStr ? *nameStr : "";
    if (funcName.empty())
    {
        compiler->Error("Native Get requires Function field");
        return nullptr;
    }

    int argCount = 0;
    const std::string* argCountStr = FindField(n, "ArgCount");
    if (argCountStr)
    {
        try { argCount = std::clamp(std::stoi(*argCountStr), 0, 6); }
        catch (...) { argCount = 0; }
    }

    std::vector<Node*> args;
    args.reserve(static_cast<size_t>(argCount));

    for (int i = 0; i < argCount; ++i)
    {
        const std::string pinName = "Arg" + std::to_string(i);
        const Pin* pin = FindInputPin(n, pinName.c_str());
        if (!pin)
            continue;
        args.push_back(BuildNumberOperand(compiler, n, *pin));
        if (compiler->HasError)
            return nullptr;
    }

    return compiler->EmitFunctionCall(funcName, std::move(args));
}
}

void NodeRegistry::RegisterDemoNodes()
{
    Register(NodeDescriptor{
        .type = NodeType::NativeCall,
        .label = "Native Call",
        .pins = {
            PinDescriptor{ .name = "In", .type = PinType::Flow, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Arg0", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Arg1", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Arg2", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Arg3", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Arg4", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Arg5", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Out", .type = PinType::Flow, .isInput = false, .isMultiInput = false },
            PinDescriptor{ .name = "Value", .type = PinType::Any, .isInput = false, .isMultiInput = false }
        },
        .fields = {
            FieldDescriptor{ .name = "Function", .valueType = PinType::String, .defaultValue = "", .options = {} },
            FieldDescriptor{ .name = "ArgCount", .valueType = PinType::String, .defaultValue = "0", .options = { "0", "1", "2", "3", "4", "5", "6" } },
            FieldDescriptor{ .name = "Arg0", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} },
            FieldDescriptor{ .name = "Arg1", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} },
            FieldDescriptor{ .name = "Arg2", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} },
            FieldDescriptor{ .name = "Arg3", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} },
            FieldDescriptor{ .name = "Arg4", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} },
            FieldDescriptor{ .name = "Arg5", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} }
        },
        .compile = CompileNativeCallNode,
        .deserialize = nullptr,
        .category = "Demo",
        .paletteVariants = {},
        .saveToken = "NativeCall",
        .deferredInputPins = { "Arg0", "Arg1", "Arg2", "Arg3", "Arg4", "Arg5" },
        .renderStyle = NodeRenderStyle::Default
    });

    Register(NodeDescriptor{
        .type = NodeType::NativeGet,
        .label = "Native Get",
        .pins = {
            PinDescriptor{ .name = "Arg0", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Arg1", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Arg2", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Arg3", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Arg4", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Arg5", .type = PinType::Number, .isInput = true, .isMultiInput = false },
            PinDescriptor{ .name = "Value", .type = PinType::Any, .isInput = false, .isMultiInput = false }
        },
        .fields = {
            FieldDescriptor{ .name = "Function", .valueType = PinType::String, .defaultValue = "", .options = {} },
            FieldDescriptor{ .name = "ArgCount", .valueType = PinType::String, .defaultValue = "0", .options = { "0", "1", "2", "3", "4", "5", "6" } },
            FieldDescriptor{ .name = "Arg0", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} },
            FieldDescriptor{ .name = "Arg1", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} },
            FieldDescriptor{ .name = "Arg2", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} },
            FieldDescriptor{ .name = "Arg3", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} },
            FieldDescriptor{ .name = "Arg4", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} },
            FieldDescriptor{ .name = "Arg5", .valueType = PinType::Number, .defaultValue = "0.0", .options = {} }
        },
        .compile = CompileNativeGetNode,
        .deserialize = nullptr,
        .category = "Demo",
        .paletteVariants = {},
        .saveToken = "NativeGet",
        .deferredInputPins = { "Arg0", "Arg1", "Arg2", "Arg3", "Arg4", "Arg5" },
        .renderStyle = NodeRenderStyle::Constant
    });
}
