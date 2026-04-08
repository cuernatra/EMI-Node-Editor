#include "../nodeRegistry.h"
#include "../../compiler/nodeCompileHelpers.h"

namespace
{
Node* CompileScopeNode(GraphCompiler*, const VisualNode&)
{
    return MakeNode(Token::Scope);
}

std::vector<std::string> CollectParamNamesFromFields(const VisualNode& fn)
{
    std::vector<std::pair<int, std::string>> ordered;
    for (const NodeField& f : fn.fields)
    {
        if (f.name.rfind("Param", 0) != 0)
            continue;

        const std::string suffix = f.name.substr(5);
        if (suffix.empty())
            continue;

        bool allDigits = true;
        for (char ch : suffix)
            allDigits = allDigits && std::isdigit(static_cast<unsigned char>(ch));
        if (!allDigits)
            continue;

        if (f.value.empty())
            continue;

        try
        {
            ordered.emplace_back(std::stoi(suffix), f.value);
        }
        catch (...) {}
    }

    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<std::string> out;
    out.reserve(ordered.size());
    for (const auto& [_, name] : ordered)
        out.push_back(name);
    return out;
}

void CompileFlowFunctionNode(GraphCompiler* compiler, const VisualNode& n, Node* targetScope)
{
    if (!compiler || !targetScope)
        return;

    if (const Pin* outFlow = FindOutputPin(n, "Out"))
        compiler->AppendFlowChainFromOutput(outFlow->id, targetScope);
}

Node* CompileOutputFunctionNode(GraphCompiler*, const VisualNode& n, int outPinIdx)
{
    if (outPinIdx < 0 || outPinIdx >= static_cast<int>(n.outPins.size()))
        return nullptr;

    const Pin& outPin = n.outPins[static_cast<size_t>(outPinIdx)];
    if (outPin.type == PinType::Flow)
        return nullptr;

    if (outPin.name == "Out")
        return nullptr;

    // Function node outputs (non-flow) are parameter identifiers inside the function body.
    return MakeIdNode(outPin.name);
}

Node* CompileTopLevelFunctionNode(GraphCompiler* compiler, const VisualNode& n)
{
    if (!compiler)
        return nullptr;

    const std::string* nameStr = FindField(n, "Name");
    const std::string funcName = (nameStr && !nameStr->empty()) ? *nameStr : "__fn";

    Node* params = MakeNode(Token::CallParams);
    for (const std::string& paramName : CollectParamNamesFromFields(n))
    {
        Node* param = MakeNode(Token::FunctionVar);
        param->data = paramName;
        param->children.push_back(MakeNode(Token::AnyType));
        params->children.push_back(param);
    }

    Node* body = MakeNode(Token::Scope);

    compiler->ResetFlowTraversalState();
    if (const Pin* outFlow = FindOutputPin(n, "Out"))
        compiler->AppendFlowChainFromOutput(outFlow->id, body);

    if (compiler->HasError)
    {
        delete params;
        delete body;
        return nullptr;
    }

    Node* funcDecl = MakeNode(Token::FunctionDef);
    funcDecl->data = funcName;
    funcDecl->children.push_back(params);
    funcDecl->children.push_back(body);
    return funcDecl;
}

bool DeserializeFunctionNode(VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds)
{
    if (pinIds.size() < desc.pins.size())
        return false;

    std::vector<int> basePins(pinIds.begin(), pinIds.begin() + static_cast<std::ptrdiff_t>(desc.pins.size()));
    if (!PopulateExactPinsAndFields(n, desc, basePins))
        return false;

    for (size_t i = desc.pins.size(); i < pinIds.size(); ++i)
    {
        const int paramIndex = static_cast<int>(i - desc.pins.size());
        n.outPins.push_back(MakePin(
            static_cast<uint32_t>(pinIds[i]),
            n.id,
            desc.type,
            "Param" + std::to_string(paramIndex),
            PinType::Any,
            false
        ));
    }

    return true;
}
}

void NodeRegistry::RegisterEventNodes()
{
    // Register(...) fields follow NodeDescriptor member order.
    Register(NodeDescriptor{
        .type = NodeType::Start,
        .label = "Start",
        .pins = {
            { "Exec", PinType::Flow, false }
        },
        .fields = {},
        .compile = CompileScopeNode,
        .deserialize = nullptr,
        .category = "Events",
        .paletteVariants = {},
        .saveToken = "Start",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Default,
    });

    Register(NodeDescriptor{
        .type = NodeType::DrawRect,
        .label = "Draw Rect",
        .pins = {
            { "In", PinType::Flow, true },
            { "X", PinType::Number, true },
            { "Y", PinType::Number, true },
            { "W", PinType::Number, true },
            { "H", PinType::Number, true },
            { "R", PinType::Number, true },
            { "G", PinType::Number, true },
            { "B", PinType::Number, true },
            { "Out", PinType::Flow, false }
        },
        .fields = {
            { "Color", PinType::String, "120, 180, 255" },
            { "X", PinType::Number, "0.0" },
            { "Y", PinType::Number, "0.0" },
            { "W", PinType::Number, "1.0" },
            { "H", PinType::Number, "1.0" },
            { "R", PinType::Number, "120.0" },
            { "G", PinType::Number, "180.0" },
            { "B", PinType::Number, "255.0" }
        },
        .compile = CompileScopeNode,
        .deserialize = nullptr,
        .category = "Events",
        .paletteVariants = {},
        .saveToken = "DrawRect",
        .deferredInputPins = { "X", "Y", "W", "H" },
        .renderStyle = NodeRenderStyle::Draw,
    });

    Register(NodeDescriptor{
        .type = NodeType::DrawGrid,
        .label = "Draw Grid",
        .pins = {
            { "In", PinType::Flow, true },
            { "X", PinType::Number, true },
            { "Y", PinType::Number, true },
            { "W", PinType::Number, true },
            { "H", PinType::Number, true },
            { "R", PinType::Number, true },
            { "G", PinType::Number, true },
            { "B", PinType::Number, true },
            { "Out", PinType::Flow, false }
        },
        .fields = {
            { "Color", PinType::String, "30, 30, 38" },
            { "X", PinType::Number, "0.0" },
            { "Y", PinType::Number, "0.0" },
            { "W", PinType::Number, "40.0" },
            { "H", PinType::Number, "60.0" },
            { "R", PinType::Number, "30.0" },
            { "G", PinType::Number, "30.0" },
            { "B", PinType::Number, "38.0" }
        },
        .compile = CompileScopeNode,
        .deserialize = nullptr,
        .category = "Events",
        .paletteVariants = {},
        .saveToken = "DrawGrid",
        .deferredInputPins = { "X", "Y", "W", "H" },
        .renderStyle = NodeRenderStyle::Draw,
    });

    Register(NodeDescriptor{
        .type = NodeType::Function,
        .label = "Function",
        .pins = {
            { "Out", PinType::Flow, false }
        },
        .fields = {
            { "Name", PinType::String, "myFunction" }
        },
        .compile = CompileScopeNode,
        .compileFlow = CompileFlowFunctionNode,
        .compileOutput = CompileOutputFunctionNode,
        .compileTopLevel = CompileTopLevelFunctionNode,
        .deserialize = DeserializeFunctionNode,
        .category = "Events",
        .paletteVariants = {},
        .saveToken = "Function",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Default,
    });
}
