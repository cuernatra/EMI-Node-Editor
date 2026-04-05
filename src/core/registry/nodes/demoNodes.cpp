#include "../nodeRegistry.h"
#include "nodeCompileHelpers.h"

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
        args.push_back(BuildNumberOperand(compiler, n, *pin, pinName.c_str()));
        if (compiler->HasError)
            return nullptr;
    }

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
        args.push_back(BuildNumberOperand(compiler, n, *pin, pinName.c_str()));
        if (compiler->HasError)
            return nullptr;
    }

    return compiler->EmitFunctionCall(funcName, std::move(args));
}
}

void NodeRegistry::RegisterDemoNodes()
{
    Register({
        NodeType::NativeCall,
        "Native Call",
        {
            { "In",   PinType::Flow,   true  },
            { "Arg0", PinType::Number, true  },
            { "Arg1", PinType::Number, true  },
            { "Arg2", PinType::Number, true  },
            { "Arg3", PinType::Number, true  },
            { "Arg4", PinType::Number, true  },
            { "Arg5", PinType::Number, true  },
            { "Out",  PinType::Flow,   false }
        },
        {
            { "Function", PinType::String, "" },
            { "ArgCount", PinType::String, "0", { "0", "1", "2", "3", "4", "5", "6" } },
            { "Arg0", PinType::Number, "0.0" },
            { "Arg1", PinType::Number, "0.0" },
            { "Arg2", PinType::Number, "0.0" },
            { "Arg3", PinType::Number, "0.0" },
            { "Arg4", PinType::Number, "0.0" },
            { "Arg5", PinType::Number, "0.0" }
        },
        CompileNativeCallNode,
        nullptr,
        "Demo",
        {},
        "NativeCall",
        { "Arg0", "Arg1", "Arg2", "Arg3", "Arg4", "Arg5" },
        NodeRenderStyle::Default
    });

    Register({
        NodeType::NativeGet,
        "Native Get",
        {
            { "Arg0",  PinType::Number, true  },
            { "Arg1",  PinType::Number, true  },
            { "Arg2",  PinType::Number, true  },
            { "Arg3",  PinType::Number, true  },
            { "Arg4",  PinType::Number, true  },
            { "Arg5",  PinType::Number, true  },
            { "Value", PinType::Any,    false }
        },
        {
            { "Function", PinType::String, "" },
            { "ArgCount", PinType::String, "0", { "0", "1", "2", "3", "4", "5", "6" } },
            { "Arg0", PinType::Number, "0.0" },
            { "Arg1", PinType::Number, "0.0" },
            { "Arg2", PinType::Number, "0.0" },
            { "Arg3", PinType::Number, "0.0" },
            { "Arg4", PinType::Number, "0.0" },
            { "Arg5", PinType::Number, "0.0" }
        },
        CompileNativeGetNode,
        nullptr,
        "Demo",
        {},
        "NativeGet",
        { "Arg0", "Arg1", "Arg2", "Arg3", "Arg4", "Arg5" },
        NodeRenderStyle::Constant
    });
}
