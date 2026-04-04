#include "graphCompiler.h"
#include "../registry/nodeRegistry.h"
#include "../registry/nodes/nodeCompileHelpers.h"

#include <algorithm>
#include <cassert>
#include <cmath>

Node* GraphCompiler::MakeNode(Token t) const
{
    Node* n = new Node();
    n->type = t;
    return n;
}

Node* GraphCompiler::MakeNumberNode(double v) const
{
    Node* n = MakeNode(Token::Number);
    n->data = v;
    return n;
}

Node* GraphCompiler::MakeBoolNode(bool v) const
{
    Node* n = MakeNode(v ? Token::True : Token::False);
    n->data = v;
    return n;
}

Node* GraphCompiler::MakeStringNode(const std::string& s) const
{
    Node* n = MakeNode(Token::Literal);
    n->data = s;
    return n;
}

Node* GraphCompiler::MakeIdNode(const std::string& name) const
{
    Node* n = MakeNode(Token::Id);
    n->data = name;
    return n;
}

void GraphCompiler::Error(const std::string& msg)
{
    errorMsg_ = msg;
    HasError = true;
}

Node* GraphCompiler::EmitBinaryOp(Token op, Node* lhs, Node* rhs) const
{
    if (!lhs || !rhs)
    {
        delete lhs;
        delete rhs;
        return nullptr;
    }

    Node* root = MakeNode(op);
    root->children.push_back(lhs);
    root->children.push_back(rhs);
    return root;
}

Node* GraphCompiler::EmitUnaryOp(Token op, Node* operand) const
{
    if (!operand)
        return nullptr;

    Node* root = MakeNode(op);
    root->children.push_back(operand);
    return root;
}

Node* GraphCompiler::EmitFunctionCall(const std::string& name, std::vector<Node*> args) const
{
    Node* call = MakeNode(Token::FunctionCall);
    call->children.push_back(MakeIdNode(name));

    Node* params = MakeNode(Token::CallParams);
    for (Node* arg : args)
    {
        if (arg)
            params->children.push_back(arg);
    }

    call->children.push_back(params);
    return call;
}

Node* GraphCompiler::EmitIndexer(Node* array, Node* index) const
{
    if (!array || !index)
    {
        delete array;
        delete index;
        return nullptr;
    }

    Node* idx = MakeNode(Token::Indexer);
    idx->children.push_back(array);
    idx->children.push_back(index);
    return idx;
}

bool GraphCompiler::NodeRequiresFlow(const VisualNode& n) const
{
    bool hasFlowIn = false;
    bool hasFlowOut = false;

    for (const Pin& p : n.inPins)
    {
        if (p.type == PinType::Flow)
        {
            hasFlowIn = true;
            break;
        }
    }

    for (const Pin& p : n.outPins)
    {
        if (p.type == PinType::Flow)
        {
            hasFlowOut = true;
            break;
        }
    }

    return hasFlowIn && hasFlowOut;
}

const VisualNode* GraphCompiler::FindFirstNode(NodeType type) const
{
    if (!nodes_)
        return nullptr;

    for (const VisualNode& n : *nodes_)
    {
        if (n.alive && n.nodeType == type)
            return &n;
    }

    return nullptr;
}

const Pin* GraphCompiler::GetOutputPinByName(const VisualNode& n, const char* name) const
{
    for (const Pin& p : n.outPins)
    {
        if (p.name == name)
            return &p;
    }
    return nullptr;
}

void GraphCompiler::CollectFlowReachableFromOutput(ed::PinId flowOutputPinId)
{
    const uintptr_t outKey = static_cast<uintptr_t>(flowOutputPinId.Get());
    if (!activeFlowOutputs_.insert(outKey).second)
        return;

    struct ScopedErase
    {
        std::unordered_set<uintptr_t>& set;
        uintptr_t key;
        ~ScopedErase() { set.erase(key); }
    } guard{ activeFlowOutputs_, outKey };

    if (!visitedFlowOutputs_.insert(outKey).second)
        return;

    const FlowTarget* target = resolver_.ResolveFlow(flowOutputPinId);
    if (!target || !target->node)
        return;

    const VisualNode& n = *target->node;
    flowReachableNodes_.insert(static_cast<uintptr_t>(n.id.Get()));

    for (const Pin& outPin : n.outPins)
    {
        if (outPin.type == PinType::Flow)
            CollectFlowReachableFromOutput(outPin.id);
    }
}

void GraphCompiler::AppendFlowChainFromOutput(ed::PinId flowOutputPinId, Node* targetScope)
{
    const uintptr_t outKey = static_cast<uintptr_t>(flowOutputPinId.Get());
    if (!activeFlowOutputs_.insert(outKey).second)
    {
        Error("Flow cycle detected while compiling execution chain");
        return;
    }

    struct ScopedErase
    {
        std::unordered_set<uintptr_t>& set;
        uintptr_t key;
        ~ScopedErase() { set.erase(key); }
    } guard{ activeFlowOutputs_, outKey };

    const FlowTarget* target = resolver_.ResolveFlow(flowOutputPinId);
    if (!target || !target->node)
        return;

    AppendFlowNode(*target->node, target->pinIdx, targetScope);
}

void GraphCompiler::AppendFlowNode(const VisualNode& n, int triggeredInputPinIdx, Node* targetScope)
{
    (void)triggeredInputPinIdx;

    if (!n.alive || !targetScope)
        return;

    switch (n.nodeType)
    {
        case NodeType::Sequence:
        {
            for (const Pin& outPin : n.outPins)
            {
                if (outPin.type != PinType::Flow)
                    continue;
                AppendFlowChainFromOutput(outPin.id, targetScope);
                if (HasError)
                    return;
            }
            return;
        }

        case NodeType::Branch:
        {
            Node* ifNode = BuildBranchNode(this, n);
            if (HasError || !ifNode)
            {
                delete ifNode;
                return;
            }

            Node* trueScope = (ifNode->children.size() > 1) ? ifNode->children[1] : nullptr;
            Node* elseScope = nullptr;
            if (ifNode->children.size() > 2 && !ifNode->children[2]->children.empty())
                elseScope = ifNode->children[2]->children[0];

            if (const Pin* trueOut = GetOutputPinByName(n, "True"))
                AppendFlowChainFromOutput(trueOut->id, trueScope);
            if (HasError)
            {
                delete ifNode;
                return;
            }

            if (const Pin* falseOut = GetOutputPinByName(n, "False"))
                AppendFlowChainFromOutput(falseOut->id, elseScope);
            if (HasError)
            {
                delete ifNode;
                return;
            }

            targetScope->children.push_back(ifNode);
            return;
        }

        case NodeType::Loop:
        {
            Node* forNode = BuildLoopNode(this, n);
            if (HasError || !forNode)
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

            if (const Pin* bodyOut = GetOutputPinByName(n, "Body"))
            {
                const uintptr_t loopKey = static_cast<uintptr_t>(n.id.Get());
                activeLoopBodyNodeIds_.insert(loopKey);
                struct ScopedErase
                {
                    std::unordered_set<uintptr_t>& set;
                    uintptr_t key;
                    ~ScopedErase() { set.erase(key); }
                } bodyGuard{ activeLoopBodyNodeIds_, loopKey };

                AppendFlowChainFromOutput(bodyOut->id, bodyScope);
            }
            if (HasError)
            {
                delete forNode;
                return;
            }

            targetScope->children.push_back(forNode);

            if (const Pin* completedOut = GetOutputPinByName(n, "Completed"))
                AppendFlowChainFromOutput(completedOut->id, targetScope);
            if (HasError)
                return;

            return;
        }

        case NodeType::While:
        {
            Node* whileNode = BuildWhileNode(this, n);
            if (HasError || !whileNode)
            {
                delete whileNode;
                return;
            }

            Node* bodyScope = (whileNode->children.size() > 1) ? whileNode->children[1] : nullptr;

            if (const Pin* bodyOut = GetOutputPinByName(n, "Body"))
                AppendFlowChainFromOutput(bodyOut->id, bodyScope);
            if (HasError)
            {
                delete whileNode;
                return;
            }

            targetScope->children.push_back(whileNode);

            if (const Pin* completedOut = GetOutputPinByName(n, "Completed"))
                AppendFlowChainFromOutput(completedOut->id, targetScope);
            if (HasError)
                return;

            return;
        }

        case NodeType::ForEach:
        {
            Node* forNode = BuildForEachNode(this, n);
            if (HasError || !forNode)
            {
                delete forNode;
                return;
            }

            Node* bodyScope = (forNode->children.size() > 1) ? forNode->children[1] : nullptr;

            if (const Pin* bodyOut = GetOutputPinByName(n, "Body"))
            {
                const uintptr_t forEachKey = static_cast<uintptr_t>(n.id.Get());
                activeForEachBodyNodeIds_.insert(forEachKey);
                struct ScopedErase
                {
                    std::unordered_set<uintptr_t>& set;
                    uintptr_t key;
                    ~ScopedErase() { set.erase(key); }
                } bodyGuard{ activeForEachBodyNodeIds_, forEachKey };

                AppendFlowChainFromOutput(bodyOut->id, bodyScope);
            }
            if (HasError)
            {
                delete forNode;
                return;
            }

            targetScope->children.push_back(forNode);

            if (const Pin* completedOut = GetOutputPinByName(n, "Completed"))
                AppendFlowChainFromOutput(completedOut->id, targetScope);
            if (HasError)
                return;

            return;
        }

        default:
            break;
    }

    Node* stmt = BuildNode(n);
    if (HasError)
    {
        delete stmt;
        return;
    }

    if (stmt)
        targetScope->children.push_back(stmt);

    const Pin* outFlow = GetOutputPinByName(n, "Out");
    if (!outFlow)
    {
        for (const Pin& p : n.outPins)
        {
            if (p.type == PinType::Flow)
            {
                outFlow = &p;
                break;
            }
        }
    }

    if (outFlow)
        AppendFlowChainFromOutput(outFlow->id, targetScope);
}

Node* GraphCompiler::Compile(const std::vector<VisualNode>& nodes,
                             const std::vector<Link>& links)
{
    HasError = false;
    errorMsg_.clear();
    nodes_ = &nodes;

    activeNodeBuilds_.clear();
    activeLoopBodyNodeIds_.clear();
    activeForEachBodyNodeIds_.clear();
    flowReachableNodes_.clear();
    activeFlowOutputs_.clear();
    visitedFlowOutputs_.clear();

    resolver_.Build(nodes, links);

    const VisualNode* startNode = FindFirstNode(NodeType::Start);
    if (!startNode)
    {
        Error("Graph requires a Start node to drive flow execution");
        return nullptr;
    }

    const Pin* startOut = GetOutputPinByName(*startNode, "Exec");
    if (!startOut)
    {
        for (const Pin& p : startNode->outPins)
        {
            if (p.type == PinType::Flow)
            {
                startOut = &p;
                break;
            }
        }
    }

    if (!startOut)
    {
        Error("Start node has no flow output pin");
        return nullptr;
    }

    CollectFlowReachableFromOutput(startOut->id);

    Node* body = MakeNode(Token::Scope);

    std::unordered_set<std::string> declaredVariables;
    for (const VisualNode& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Variable)
            continue;

        const std::string* variant = FindField(n, "Variant");
        const bool isGet = (variant && *variant == "Get");
        if (isGet)
            continue;

        const std::string* nameStr = FindField(n, "Name");
        const std::string varName = (nameStr && !nameStr->empty()) ? *nameStr : "__unnamed";
        if (!declaredVariables.insert(varName).second)
            continue;

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
        body->children.push_back(decl);
    }

    for (const VisualNode& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Loop)
            continue;

        double startVal = 0.0;
        const Pin* startPin = FindInputPin(n, "Start");
        const bool startConnected = (startPin && resolver_.Resolve(startPin->id));
        if (!startConnected)
        {
            const std::string* startStr = FindField(n, "Start");
            if (startStr)
            {
                try
                {
                    startVal = std::stod(*startStr);
                }
                catch (...)
                {
                    startVal = 0.0;
                }
            }
        }

        Node* loopLastDecl = MakeNode(Token::VarDeclare);
        loopLastDecl->data = LoopLastIndexVarNameForNode(n);
        loopLastDecl->children.push_back(MakeNode(Token::TypeNumber));
        loopLastDecl->children.push_back(MakeNumberNode(std::round(startVal)));
        body->children.push_back(loopLastDecl);
    }

    visitedFlowOutputs_.clear();
    activeFlowOutputs_.clear();
    AppendFlowChainFromOutput(startOut->id, body);
    if (HasError)
    {
        delete body;
        return nullptr;
    }

    Node* funcDecl = MakeNode(Token::FunctionDef);
    funcDecl->data = std::string(kGraphFunctionName);
    funcDecl->children.push_back(MakeNode(Token::CallParams));
    funcDecl->children.push_back(body);

    Node* root = MakeNode(Token::Scope);
    root->children.push_back(funcDecl);
    return root;
}

Node* GraphCompiler::BuildExpr(const Pin& inputPin)
{
    auto makeDefaultValue = [&]() -> Node*
    {
        switch (inputPin.type)
        {
            case PinType::Number:
                return MakeNumberNode(0.0);
            case PinType::Boolean:
                return MakeBoolNode(false);
            case PinType::String:
                return MakeStringNode("");
            case PinType::Array:
                return MakeNode(Token::Array);
            default:
                return MakeNode(Token::Null);
        }
    };

    const PinSource* src = resolver_.Resolve(inputPin.id);
    if (!src)
        return makeDefaultValue();

    if (src->node && NodeRequiresFlow(*src->node))
    {
        const uintptr_t key = static_cast<uintptr_t>(src->node->id.Get());
        if (flowReachableNodes_.find(key) == flowReachableNodes_.end())
            return makeDefaultValue();
    }

    return BuildNode(*src->node, src->pinIdx);
}

Node* GraphCompiler::BuildNode(const VisualNode& n, int outPinIdx)
{
    const uintptr_t nodeKey = static_cast<uintptr_t>(n.id.Get());
    if (!activeNodeBuilds_.insert(nodeKey).second)
    {
        Error("Cycle detected while compiling node: " + n.title);
        return nullptr;
    }

    struct ScopedErase
    {
        std::unordered_set<uintptr_t>& set;
        uintptr_t key;
        ~ScopedErase() { set.erase(key); }
    } guard{ activeNodeBuilds_, nodeKey };

    if (n.nodeType == NodeType::Loop && outPinIdx >= 0 && outPinIdx < static_cast<int>(n.outPins.size()))
    {
        const Pin& requestedOutPin = n.outPins[static_cast<size_t>(outPinIdx)];
        if (requestedOutPin.type != PinType::Flow && requestedOutPin.name == "Index")
        {
            const uintptr_t loopKey = static_cast<uintptr_t>(n.id.Get());
            const bool readingInsideLoopBody =
                (activeLoopBodyNodeIds_.find(loopKey) != activeLoopBodyNodeIds_.end());
            return MakeIdNode(readingInsideLoopBody
                ? LoopIndexVarNameForNode(n)
                : LoopLastIndexVarNameForNode(n));
        }
    }

    if (n.nodeType == NodeType::ForEach && outPinIdx >= 0 && outPinIdx < static_cast<int>(n.outPins.size()))
    {
        const Pin& requestedOutPin = n.outPins[static_cast<size_t>(outPinIdx)];
        if (requestedOutPin.type != PinType::Flow && requestedOutPin.name == "Element")
            return MakeIdNode(ForEachElementVarNameForNode(n));
    }

    const NodeDescriptor* descriptor = NodeRegistry::Get().Find(n.nodeType);
    if (descriptor && descriptor->compile)
        return descriptor->compile(this, n);

    Error("No compile callback registered for NodeType");
    return nullptr;
}

Node* GraphCompiler::BuildArrayLiteralNode(const std::string& text) const
{
    std::string s = TrimCopy(text);
    if (s.empty())
        return MakeNode(Token::Array);

    if (s.size() >= 2 && s.front() == '[' && s.back() == ']')
        s = s.substr(1, s.size() - 2);

    Node* arr = MakeNode(Token::Array);
    const std::vector<std::string> items = SplitArrayItemsTopLevel(s);
    for (const std::string& raw : items)
    {
        const std::string item = TrimCopy(raw);
        if (item.empty())
            continue;

        if ((item.front() == '"' && item.back() == '"') ||
            (item.front() == '\'' && item.back() == '\''))
        {
            arr->children.push_back(MakeStringNode(item.substr(1, item.size() - 2)));
            continue;
        }

        if (item == "true" || item == "True")
        {
            arr->children.push_back(MakeBoolNode(true));
            continue;
        }

        if (item == "false" || item == "False")
        {
            arr->children.push_back(MakeBoolNode(false));
            continue;
        }

        if (item == "null" || item == "Null")
        {
            arr->children.push_back(MakeNode(Token::Null));
            continue;
        }

        if (item.front() == '[' && item.back() == ']')
        {
            arr->children.push_back(BuildArrayLiteralNode(item));
            continue;
        }

        double numeric = 0.0;
        if (TryParseDoubleExact(item, numeric))
        {
            arr->children.push_back(MakeNumberNode(numeric));
            continue;
        }

        arr->children.push_back(MakeStringNode(item));
    }

    return arr;
}

// ???????????????????????????
void GraphCompiler::AppendFlowFromPin(const Pin& outputPin, Node* targetScope)
{
    AppendFlowChainFromOutput(outputPin.id, targetScope);
}
