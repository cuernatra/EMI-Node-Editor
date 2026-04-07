#include "../nodeRegistry.h"
#include "../../compiler/nodeCompileHelpers.h"

// Forward declarations so the anonymous-namespace compileFlow callbacks can call
// these file-scoped builders (defined after the namespace closes).
Node* CompileBranchNode(GraphCompiler*, const VisualNode&);
Node* CompileLoopNode  (GraphCompiler*, const VisualNode&);
Node* CompileForEachNode(GraphCompiler*, const VisualNode&);
Node* CompileWhileNode  (GraphCompiler*, const VisualNode&);

namespace
{
struct ScopedLoopBodyGuard
{
    GraphCompiler* compiler;
    uintptr_t key;
    ScopedLoopBodyGuard(GraphCompiler* c, uintptr_t k) : compiler(c), key(k)
    {
        if (compiler)
            compiler->PushActiveLoopBodyNodeId(key);
    }
    ~ScopedLoopBodyGuard()
    {
        if (compiler)
            compiler->PopActiveLoopBodyNodeId(key);
    }
    ScopedLoopBodyGuard(const ScopedLoopBodyGuard&) = delete;
    ScopedLoopBodyGuard& operator=(const ScopedLoopBodyGuard&) = delete;
};

struct ScopedForEachBodyGuard
{
    GraphCompiler* compiler;
    uintptr_t key;
    ScopedForEachBodyGuard(GraphCompiler* c, uintptr_t k) : compiler(c), key(k)
    {
        if (compiler)
            compiler->PushActiveForEachBodyNodeId(key);
    }
    ~ScopedForEachBodyGuard()
    {
        if (compiler)
            compiler->PopActiveForEachBodyNodeId(key);
    }
    ScopedForEachBodyGuard(const ScopedForEachBodyGuard&) = delete;
    ScopedForEachBodyGuard& operator=(const ScopedForEachBodyGuard&) = delete;
};

void CompileFlowTickerNode(GraphCompiler* compiler, const VisualNode& n, Node* targetScope)
{
    Node* tickerNode = compiler->BuildNode(n);
    if (compiler->HasError || !tickerNode)
    {
        delete tickerNode;
        return;
    }

    targetScope->children.push_back(tickerNode);
}

void CompileFlowSequenceNode(GraphCompiler* compiler, const VisualNode& n, Node* targetScope)
{
    // Sequence forwards execution through each flow output in order.
    for (const Pin& outPin : n.outPins)
    {
        if (outPin.type != PinType::Flow)
            continue;

        compiler->AppendFlowChainFromOutput(outPin.id, targetScope);
        if (compiler->HasError)
            return;
    }
}

void CompileFlowBranchNode(GraphCompiler* compiler, const VisualNode& n, Node* targetScope)
{
    Node* ifNode = CompileBranchNode(compiler, n);
    if (compiler->HasError || !ifNode)
    {
        delete ifNode;
        return;
    }

    Node* trueScope = (ifNode->children.size() > 1) ? ifNode->children[1] : nullptr;
    Node* elseScope = nullptr;
    if (ifNode->children.size() > 2 && !ifNode->children[2]->children.empty())
        elseScope = ifNode->children[2]->children[0];

    if (const Pin* trueOut = FindOutputPin(n, "True"))
        compiler->AppendFlowChainFromOutput(trueOut->id, trueScope);
    if (compiler->HasError)
    {
        delete ifNode;
        return;
    }

    if (const Pin* falseOut = FindOutputPin(n, "False"))
        compiler->AppendFlowChainFromOutput(falseOut->id, elseScope);
    if (compiler->HasError)
    {
        delete ifNode;
        return;
    }

    targetScope->children.push_back(ifNode);
}

void CompileFlowLoopNode(GraphCompiler* compiler, const VisualNode& n, Node* targetScope)
{
    Node* forNode = CompileLoopNode(compiler, n);
    if (compiler->HasError || !forNode)
    {
        delete forNode;
        return;
    }

    Node* bodyScope = nullptr;
    if (forNode->type == Token::For)
    {
        bodyScope = (forNode->children.size() > 3) ? forNode->children[3] : nullptr;
    }
    else if (forNode->type == Token::Scope)
    {
        for (auto it = forNode->children.rbegin(); it != forNode->children.rend(); ++it)
        {
            Node* child = *it;
            if (!child || child->type != Token::For)
                continue;

            bodyScope = (child->children.size() > 3) ? child->children[3] : nullptr;
            break;
        }
    }

    if (const Pin* bodyOut = FindOutputPin(n, "Body"))
    {
        const uintptr_t loopKey = static_cast<uintptr_t>(n.id.Get());
        ScopedLoopBodyGuard bodyGuard{ compiler, loopKey };
        compiler->AppendFlowChainFromOutput(bodyOut->id, bodyScope);
    }
    if (compiler->HasError)
    {
        delete forNode;
        return;
    }

    targetScope->children.push_back(forNode);

    if (const Pin* completedOut = FindOutputPin(n, "Completed"))
        compiler->AppendFlowChainFromOutput(completedOut->id, targetScope);
    if (compiler->HasError)
        return;
}

void CompileFlowWhileNode(GraphCompiler* compiler, const VisualNode& n, Node* targetScope)
{
    Node* whileNode = CompileWhileNode(compiler, n);
    if (compiler->HasError || !whileNode)
    {
        delete whileNode;
        return;
    }

    Node* bodyScope = (whileNode->children.size() > 1) ? whileNode->children[1] : nullptr;

    if (const Pin* bodyOut = FindOutputPin(n, "Body"))
        compiler->AppendFlowChainFromOutput(bodyOut->id, bodyScope);
    if (compiler->HasError)
    {
        delete whileNode;
        return;
    }

    targetScope->children.push_back(whileNode);

    if (const Pin* completedOut = FindOutputPin(n, "Completed"))
        compiler->AppendFlowChainFromOutput(completedOut->id, targetScope);
    if (compiler->HasError)
        return;
}

void CompileFlowForEachNode(GraphCompiler* compiler, const VisualNode& n, Node* targetScope)
{
    Node* forNode = CompileForEachNode(compiler, n);
    if (compiler->HasError || !forNode)
    {
        delete forNode;
        return;
    }

    Node* bodyScope = (forNode->children.size() > 1) ? forNode->children[1] : nullptr;

    if (const Pin* bodyOut = FindOutputPin(n, "Body"))
    {
        const uintptr_t forEachKey = static_cast<uintptr_t>(n.id.Get());
        ScopedForEachBodyGuard bodyGuard{ compiler, forEachKey };
        compiler->AppendFlowChainFromOutput(bodyOut->id, bodyScope);
    }
    if (compiler->HasError)
    {
        delete forNode;
        return;
    }

    targetScope->children.push_back(forNode);

    if (const Pin* completedOut = FindOutputPin(n, "Completed"))
        compiler->AppendFlowChainFromOutput(completedOut->id, targetScope);
    if (compiler->HasError)
        return;
}

Node* CompileOutputLoopNode(GraphCompiler* compiler, const VisualNode& n, int outPinIdx)
{
    if (!compiler)
        return nullptr;

    if (outPinIdx < 0 || outPinIdx >= static_cast<int>(n.outPins.size()))
        return nullptr;

    const Pin& outPin = n.outPins[static_cast<size_t>(outPinIdx)];
    if (outPin.type == PinType::Flow)
        return nullptr;

    if (outPin.name != "Index")
        return nullptr;

    const uintptr_t loopKey = static_cast<uintptr_t>(n.id.Get());
    return MakeIdNode(compiler->IsActiveLoopBodyNodeId(loopKey)
        ? LoopIndexVarNameForNode(n)
        : LoopLastIndexVarNameForNode(n));
}

Node* CompileOutputForEachNode(GraphCompiler*, const VisualNode& n, int outPinIdx)
{
    if (outPinIdx < 0 || outPinIdx >= static_cast<int>(n.outPins.size()))
        return nullptr;

    const Pin& outPin = n.outPins[static_cast<size_t>(outPinIdx)];
    if (outPin.type == PinType::Flow)
        return nullptr;

    if (outPin.name != "Element")
        return nullptr;

    return MakeIdNode(ForEachElementVarNameForNode(n));
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

void CompilePreludeLoopNode(GraphCompiler* compiler, const VisualNode& n, Node*, Node* graphBodyScope)
{
    if (!compiler || !graphBodyScope)
        return;

    double startVal = 0.0;
    const Pin* startPin = FindInputPin(n, "Start");
    const bool startConnected = (startPin && compiler->Resolve(*startPin));
    if (!startConnected)
    {
        const std::string* startStr = FindField(n, "Start");
        if (startStr)
        {
            try { startVal = std::stod(*startStr); }
            catch (...) { startVal = 0.0; }
        }
    }

    Node* loopLastDecl = MakeNode(Token::VarDeclare);
    loopLastDecl->data = LoopLastIndexVarNameForNode(n);
    loopLastDecl->children.push_back(MakeNode(Token::TypeNumber));
    loopLastDecl->children.push_back(MakeNumberLiteral(std::round(startVal)));
    graphBodyScope->children.push_back(loopLastDecl);
}

void CompilePreludeCallFunctionNode(GraphCompiler*, const VisualNode& n, Node* rootScope, Node*)
{
    if (!rootScope)
        return;

    const std::string tempVar = "__call_result_" +
        std::to_string(static_cast<uintptr_t>(n.id.Get()));

    Node* decl = MakeNode(Token::VarDeclare);
    decl->data = tempVar;
    decl->children.push_back(MakeNode(Token::AnyType));
    rootScope->children.push_back(decl);
}

Node* CompileOutputCallFunctionNode(GraphCompiler*, const VisualNode& n, int outPinIdx)
{
    if (outPinIdx < 0 || outPinIdx >= static_cast<int>(n.outPins.size()))
        return nullptr;

    const Pin& outPin = n.outPins[static_cast<size_t>(outPinIdx)];
    if (outPin.type == PinType::Flow)
        return nullptr;

    if (outPin.name != "Result")
        return nullptr;

    const std::string tempVar = "__call_result_" +
        std::to_string(static_cast<uintptr_t>(n.id.Get()));
    return MakeIdNode(tempVar);
}

Node* CompileCallFunctionNode(GraphCompiler* compiler, const VisualNode& n)
{
    if (!compiler)
        return nullptr;

    const std::string* nameStr = FindField(n, "Name");
    const std::string funcName = (nameStr && !nameStr->empty()) ? *nameStr : "";
    if (funcName.empty())
    {
        compiler->Error("CallFunction node is missing function Name");
        return nullptr;
    }

    const VisualNode* targetFn = compiler->FindUserFunctionByName(funcName);
    if (!targetFn)
    {
        compiler->Error("CallFunction target not found: " + funcName);
        return nullptr;
    }

    const std::vector<std::string> paramOrder = CollectParamNamesFromFields(*targetFn);

    Node* callParams = MakeNode(Token::CallParams);
    for (const std::string& paramName : paramOrder)
    {
        const Pin* argPin = nullptr;
        for (const Pin& p : n.inPins)
        {
            if (p.type == PinType::Flow)
                continue;
            if (p.name == paramName)
            {
                argPin = &p;
                break;
            }
        }

        Node* argExpr = argPin ? compiler->BuildExpr(*argPin) : MakeNode(Token::Null);
        if (compiler->HasError || !argExpr)
        {
            delete callParams;
            return nullptr;
        }

        callParams->children.push_back(argExpr);
    }

    Node* call = MakeNode(Token::FunctionCall);
    call->children.push_back(MakeIdNode(funcName));
    call->children.push_back(callParams);

    const std::string tempVar = "__call_result_" + std::to_string(static_cast<uintptr_t>(n.id.Get()));
    Node* assign = MakeNode(Token::Assign);
    assign->children.push_back(MakeIdNode(tempVar));
    assign->children.push_back(call);
    return assign;
}
}


// Move CompileTickerNode out of anonymous namespace for linker visibility
Node* CompileTickerNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* fpsPin = FindInputPin(n, "FPS");
    if (!fpsPin)
    {
        compiler->Error("Ticker node needs FPS input");
        return nullptr;
    }

    Node* fps = BuildNumberOperand(compiler, n, *fpsPin);
    if (!fps)
        return nullptr;

    Node* body = MakeNode(Token::Scope);
    Node* loop = MakeNode(Token::While);
    loop->children.push_back(MakeBoolLiteral(true));
    loop->children.push_back(body);

    if (const Pin* tick = FindOutputPin(n, "Tick"))
        compiler->AppendFlowChainFromOutput(tick->id, body);

    // Delay appended after user's body chain, inside the loop scope.
    body->children.push_back(
        compiler->EmitFunctionCall("delay", { compiler->EmitBinaryOp(Token::Div, MakeNumberLiteral(1000.0), fps) })
    );

    Node* targetScope = MakeNode(Token::Scope);
    targetScope->children.push_back(loop);

    if (const Pin* stop = FindOutputPin(n, "Stop"))
        compiler->AppendFlowChainFromOutput(stop->id, targetScope);

    return targetScope;
}

// End anonymous namespace

// The Compile* helpers below are used by both:
// - compileFlow callbacks above (inside the anonymous namespace)
// - NodeDescriptor::compile callbacks in the registry

Node* CompileBranchNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* conditionPin = FindInputPin(n, "Condition");
    if (!conditionPin)
    {
        compiler->Error("Branch node needs Condition input");
        return nullptr;
    }

    Node* ifNode = MakeNode(Token::If);
    Node* condExpr = compiler->BuildExpr(*conditionPin);
    if (compiler->HasError) { delete ifNode; return nullptr; }

    // If the source pin carries a Number or Any value, wrap with != 0 so that
    // native functions returning 1.0/0.0 work as boolean conditions.
    const PinSource* condSrc = compiler->Resolve(*conditionPin);
    if (condSrc && condSrc->node && condSrc->pinIdx >= 0 &&
        condSrc->pinIdx < static_cast<int>(condSrc->node->outPins.size()))
    {
        const PinType srcType = condSrc->node->outPins[static_cast<size_t>(condSrc->pinIdx)].type;
        if (srcType == PinType::Number || srcType == PinType::Any)
        {
            Node* notEq = MakeNode(Token::NotEqual);
            notEq->children.push_back(condExpr);
            notEq->children.push_back(MakeNumberLiteral(0.0));
            condExpr = notEq;
        }
    }

    ifNode->children.push_back(condExpr);
    ifNode->children.push_back(MakeNode(Token::Scope));

    if (FindOutputPin(n, "False"))
    {
        Node* elseScope = MakeNode(Token::Scope);
        Node* elseNode = MakeNode(Token::Else);
        elseNode->children.push_back(elseScope);
        ifNode->children.push_back(elseNode);
    }

    return ifNode;
}

Node* CompileLoopNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* startPin = FindInputPin(n, "Start");
    const Pin* countPin = FindInputPin(n, "Count");
    if (!countPin)
    {
        compiler->Error("Loop node needs Count input");
        return nullptr;
    }

    Node* startExpr = startPin ? BuildNumberInput(compiler, n, *startPin) : nullptr;
    if (!startExpr)
        startExpr = MakeNumberLiteral(0.0);
    else if (startExpr->type == Token::Number)
    {
        if (double* v = std::get_if<double>(&startExpr->data))
            *v = std::round(*v);
    }

    Node* countExpr = BuildNumberInput(compiler, n, *countPin);
    if (countExpr && countExpr->type == Token::Number)
    {
        if (double* v = std::get_if<double>(&countExpr->data))
            *v = std::round(*v);
    }

    if (compiler->HasError)
    {
        delete startExpr;
        delete countExpr;
        return nullptr;
    }

    const std::string indexVarName     = LoopIndexVarNameForNode(n);
    const std::string lastIndexVarName = LoopLastIndexVarNameForNode(n);
    const std::string startVarName     = LoopStartVarNameForNode(n);
    const std::string endVarName       = LoopEndVarNameForNode(n);

    Node* prelude = MakeNode(Token::Scope);

    Node* startDecl = MakeNode(Token::VarDeclare);
    startDecl->data = startVarName;
    startDecl->children.push_back(MakeNode(Token::TypeNumber));
    startDecl->children.push_back(startExpr);
    prelude->children.push_back(startDecl);

    Node* endExpr = MakeNode(Token::Add);
    endExpr->children.push_back(MakeIdNode(startVarName));
    endExpr->children.push_back(countExpr);

    Node* endDecl = MakeNode(Token::VarDeclare);
    endDecl->data = endVarName;
    endDecl->children.push_back(MakeNode(Token::TypeNumber));
    endDecl->children.push_back(endExpr);
    prelude->children.push_back(endDecl);

    Node* varDecl = MakeNode(Token::VarDeclare);
    varDecl->data = indexVarName;
    varDecl->children.push_back(MakeNode(Token::TypeNumber));
    varDecl->children.push_back(MakeIdNode(startVarName));

    Node* cond = MakeNode(Token::Less);
    cond->children.push_back(MakeIdNode(indexVarName));
    cond->children.push_back(MakeIdNode(endVarName));

    Node* incr = MakeNode(Token::Increment);
    incr->children.push_back(MakeIdNode(indexVarName));

    Node* body = MakeNode(Token::Scope);
    Node* cacheAssign = MakeNode(Token::Assign);
    cacheAssign->children.push_back(MakeIdNode(lastIndexVarName));
    cacheAssign->children.push_back(MakeIdNode(indexVarName));
    body->children.push_back(cacheAssign);

    Node* forNode = MakeNode(Token::For);
    forNode->children.push_back(varDecl);
    forNode->children.push_back(cond);
    forNode->children.push_back(incr);
    forNode->children.push_back(body);
    forNode->children.push_back(MakeNode(Token::Scope)); // Completed scope placeholder

    prelude->children.push_back(forNode);
    return prelude;
}

Node* CompileForEachNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* arrayPin = FindInputPin(n, "Array");
    if (!arrayPin)
    {
        compiler->Error("For Each node needs Array input");
        return nullptr;
    }

    Node* arrayExpr = BuildArrayInput(compiler, n, *arrayPin);
    if (compiler->HasError || !arrayExpr)
    {
        delete arrayExpr;
        return nullptr;
    }

    Node* loop = MakeNode(Token::For);
    loop->data = ForEachElementVarNameForNode(n);
    loop->children.push_back(arrayExpr);
    loop->children.push_back(MakeNode(Token::Scope)); // Body
    loop->children.push_back(MakeNode(Token::Scope)); // Completed
    return loop;
}

Node* CompileWhileNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* conditionPin = FindInputPin(n, "Condition");
    if (!conditionPin)
    {
        compiler->Error("While node needs Condition input");
        return nullptr;
    }

    Node* condExpr = compiler->BuildExpr(*conditionPin);
    if (compiler->HasError || !condExpr)
        return nullptr;

    Node* whileNode = MakeNode(Token::While);
    whileNode->children.push_back(condExpr);
    whileNode->children.push_back(MakeNode(Token::Scope));
    return whileNode;
}

Node* CompileDelayNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* durationPin = FindInputPin(n, "Duration");
    if (!durationPin)
    {
        compiler->Error("Delay node needs Duration input");
        return nullptr;
    }

    Node* durationExpr = BuildNumberOperand(compiler, n, *durationPin);
    if (!durationExpr)
        return nullptr;

    return compiler->EmitFunctionCall("delay", { durationExpr });
}

Node* CompileSequenceNode(GraphCompiler*, const VisualNode&)
{
    return MakeNode(Token::Scope);
}

bool DeserializeSequenceNode(VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds)
{
    if (pinIds.size() < desc.pins.size())
        return false;

    std::vector<int> basePins(pinIds.begin(), pinIds.begin() + static_cast<std::ptrdiff_t>(desc.pins.size()));
    if (!PopulateExactPinsAndFields(n, desc, basePins))
        return false;

    for (size_t i = desc.pins.size(); i < pinIds.size(); ++i)
    {
        const int thenIndex = static_cast<int>(n.outPins.size());
        n.outPins.push_back(MakePin(
            static_cast<uint32_t>(pinIds[i]),
            n.id,
            desc.type,
            "Then " + std::to_string(thenIndex),
            PinType::Flow,
            false
        ));
    }

    return true;
}

bool DeserializeCallFunctionNode(VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds)
{
    if (pinIds.size() < desc.pins.size())
        return false;

    std::vector<int> basePins(pinIds.begin(), pinIds.begin() + static_cast<std::ptrdiff_t>(desc.pins.size()));
    if (!PopulateExactPinsAndFields(n, desc, basePins))
        return false;

    // Extra pins are argument inputs.
    for (size_t i = desc.pins.size(); i < pinIds.size(); ++i)
    {
        const int argIndex = static_cast<int>(i - desc.pins.size());
        n.inPins.push_back(MakePin(
            static_cast<uint32_t>(pinIds[i]),
            n.id,
            desc.type,
            "Arg" + std::to_string(argIndex),
            PinType::Any,
            true
        ));
    }

    return true;
}

void NodeRegistry::RegisterFlowNodes()
{

    Register(NodeDescriptor{
        .type = NodeType::Ticker,
        .label = "Ticker",
        .pins = {
            { "In", PinType::Flow, true },
            { "FPS", PinType::Number, true },
            { "Tick", PinType::Flow, false },
            { "Stop", PinType::Flow, false }
        },
        .fields = {
            { "FPS", PinType::Number, "20" }
        },
        .compile = CompileTickerNode,
        .compileFlow = CompileFlowTickerNode,
        .deserialize = nullptr,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "Ticker",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Default,
    });

    Register(NodeDescriptor{
        .type = NodeType::Delay,
        .label = "Delay",
        .pins = {
            { "In", PinType::Flow, true },
            { "Duration", PinType::Number, true },
            { "Out", PinType::Flow, false }
        },
        .fields = {
            { "Duration", PinType::Number, "1000.0" }
        },
        .compile = CompileDelayNode,
        .compileFlow = nullptr,
        .deserialize = nullptr,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "Delay",
        .deferredInputPins = { "Duration" },
        .renderStyle = NodeRenderStyle::Delay,
    });

    Register(NodeDescriptor{
        .type = NodeType::Sequence,
        .label = "Sequence",
        .pins = {
            { "In", PinType::Flow, true },
            { "Then 0", PinType::Flow, false }
        },
        .fields = {},
        .compile = CompileSequenceNode,
        .compileFlow = CompileFlowSequenceNode,
        .deserialize = DeserializeSequenceNode,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "Sequence",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Sequence,
    });

    Register(NodeDescriptor{
        .type = NodeType::Branch,
        .label = "Branch",
        .pins = {
            { "In", PinType::Flow, true },
            { "Condition", PinType::Boolean, true },
            { "True", PinType::Flow, false },
            { "False", PinType::Flow, false }
        },
        .fields = {},
        .compile = CompileBranchNode,
        .compileFlow = CompileFlowBranchNode,
        .deserialize = nullptr,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "Branch",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Default,
    });

    Register(NodeDescriptor{
        .type = NodeType::CallFunction,
        .label = "Call Function",
        .pins = {
            { "In", PinType::Flow, true },
            { "Out", PinType::Flow, false },
            { "Result", PinType::Any, false }
        },
        .fields = {
            { "Name", PinType::String, "" }
        },
        .compile = CompileCallFunctionNode,
        .compileFlow = nullptr,
        .compileOutput = CompileOutputCallFunctionNode,
        .compilePrelude = CompilePreludeCallFunctionNode,
        .deserialize = DeserializeCallFunctionNode,
        .bypassFlowReachableCheck = true,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "CallFunction",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Default,
    });

    Register(NodeDescriptor{
        .type = NodeType::Loop,
        .label = "Loop",
        .pins = {
            { "In", PinType::Flow, true },
            { "Start", PinType::Number, true },
            { "Count", PinType::Number, true },
            { "Body", PinType::Flow, false },
            { "Completed", PinType::Flow, false },
            { "Index", PinType::Number, false }
        },
        .fields = {
            { "Start", PinType::Number, "0" },
            { "Count", PinType::Number, "0" }
        },
        .compile = CompileLoopNode,
        .compileFlow = CompileFlowLoopNode,
        .compileOutput = CompileOutputLoopNode,
        .compilePrelude = CompilePreludeLoopNode,
        .deserialize = nullptr,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "Loop",
        .deferredInputPins = { "Start", "Count" },
        .renderStyle = NodeRenderStyle::Loop,
    });

    Register(NodeDescriptor{
        .type = NodeType::ForEach,
        .label = "For Each",
        .pins = {
            { "In", PinType::Flow, true },
            { "Array", PinType::Array, true },
            { "Body", PinType::Flow, false },
            { "Completed", PinType::Flow, false },
            { "Element", PinType::Any, false }
        },
        .fields = {
            { "Element Type", PinType::String, "Any", { "Any", "Number", "Boolean", "String", "Array" } },
            { "Array", PinType::Array, "[]" }
        },
        .compile = CompileForEachNode,
        .compileFlow = CompileFlowForEachNode,
        .compileOutput = CompileOutputForEachNode,
        .deserialize = nullptr,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "ForEach",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::ForEach,
    });

    Register(NodeDescriptor{
        .type = NodeType::While,
        .label = "While",
        .pins = {
            { "In", PinType::Flow, true },
            { "Condition", PinType::Boolean, true },
            { "Body", PinType::Flow, false },
            { "Completed", PinType::Flow, false }
        },
        .fields = {},
        .compile = CompileWhileNode,
        .compileFlow = CompileFlowWhileNode,
        .deserialize = nullptr,
        .category = "Flow",
        .paletteVariants = {},
        .saveToken = "While",
        .deferredInputPins = {},
        .renderStyle = NodeRenderStyle::Default,
    });
}
