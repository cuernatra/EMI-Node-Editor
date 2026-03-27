#include "graphCompiler.h"
#include "../registry/nodeRegistry.h"
#include <stdexcept>
#include <cassert>
#include <cmath>


// Helpers

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

const std::string* GraphCompiler::GetField(const VisualNode& n,
                                            const std::string& name) const
{
    for (const NodeField& f : n.fields)
        if (f.name == name) return &f.value;
    return nullptr;
}

const Pin* GraphCompiler::GetInputPinByName(const VisualNode& n, const char* name) const
{
    for (const Pin& p : n.inPins)
        if (p.name == name) return &p;
    return nullptr;
}

void GraphCompiler::Error(const std::string& msg)
{
    errorMsg_ = msg;
    HasError  = true;
}

// Token mappings

Token GraphCompiler::OperatorToken(const std::string& op)
{
    if (op == "+") return Token::Add;
    if (op == "-") return Token::Sub;
    if (op == "*") return Token::Mult;
    if (op == "/") return Token::Div;
    return Token::None;
}

Token GraphCompiler::CompareToken(const std::string& op)
{
    if (op == "==") return Token::Equal;
    if (op == "!=") return Token::NotEqual;
    if (op == "<")  return Token::Less;
    if (op == "<=") return Token::LessEqual;
    if (op == ">")  return Token::Larger;
    if (op == ">=") return Token::LargerEqual;
    return Token::None;
}

Token GraphCompiler::LogicToken(const std::string& op)
{
    if (op == "AND") return Token::And;
    if (op == "OR")  return Token::Or;
    return Token::None;
}

static PinType VariableTypeFromString(const std::string& typeName)
{
    if (typeName == "Boolean") return PinType::Boolean;
    if (typeName == "String")  return PinType::String;
    if (typeName == "Array")   return PinType::Array;
    return PinType::Number;
}

bool GraphCompiler::NodeRequiresFlow(const VisualNode& n) const
{
    bool hasFlowIn = false;
    bool hasFlowOut = false;

    for (const Pin& p : n.inPins)
        if (p.type == PinType::Flow) { hasFlowIn = true; break; }
    for (const Pin& p : n.outPins)
        if (p.type == PinType::Flow) { hasFlowOut = true; break; }

    return hasFlowIn && hasFlowOut;
}

const VisualNode* GraphCompiler::FindFirstNode(NodeType type) const
{
    if (!nodes_) return nullptr;
    for (const VisualNode& n : *nodes_)
        if (n.alive && n.nodeType == type)
            return &n;
    return nullptr;
}

const Pin* GraphCompiler::GetOutputPinByName(const VisualNode& n, const char* name) const
{
    for (const Pin& p : n.outPins)
        if (p.name == name)
            return &p;
    return nullptr;
}

std::string GraphCompiler::LoopIndexVarName(const VisualNode& n) const
{
    return "__loop_i_" + std::to_string(static_cast<unsigned long long>(static_cast<uintptr_t>(n.id.Get())));
}

std::string GraphCompiler::LoopLastIndexVarName(const VisualNode& n) const
{
    return "__loop_last_i_" + std::to_string(static_cast<unsigned long long>(static_cast<uintptr_t>(n.id.Get())));
}

void GraphCompiler::CollectFlowReachableFromOutput(ed::PinId flowOutputPinId)
{
    const uintptr_t outKey = static_cast<uintptr_t>(flowOutputPinId.Get());
    if (!activeFlowOutputs_.insert(outKey).second)
        return;

    struct ScopedErase {
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

    struct ScopedErase {
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
                if (HasError) return;
            }
            return;
        }

        case NodeType::Branch:
        {
            Node* ifNode = BuildBranch(n);
            if (HasError || !ifNode) { delete ifNode; return; }

            Node* trueScope = (ifNode->children.size() > 1) ? ifNode->children[1] : nullptr;
            Node* elseScope = nullptr;
            if (ifNode->children.size() > 2 && !ifNode->children[2]->children.empty())
                elseScope = ifNode->children[2]->children[0];

            if (const Pin* trueOut = GetOutputPinByName(n, "True"))
                AppendFlowChainFromOutput(trueOut->id, trueScope);
            if (HasError) { delete ifNode; return; }

            if (const Pin* falseOut = GetOutputPinByName(n, "False"))
                AppendFlowChainFromOutput(falseOut->id, elseScope);
            if (HasError) { delete ifNode; return; }

            targetScope->children.push_back(ifNode);
            return;
        }

        case NodeType::Loop:
        {
            Node* forNode = BuildLoop(n);
            if (HasError || !forNode) { delete forNode; return; }

            Node* bodyScope = (forNode->children.size() > 3) ? forNode->children[3] : nullptr;

            if (const Pin* bodyOut = GetOutputPinByName(n, "Body"))
            {
                const uintptr_t loopKey = static_cast<uintptr_t>(n.id.Get());
                activeLoopBodyNodeIds_.insert(loopKey);
                struct ScopedErase {
                    std::unordered_set<uintptr_t>& set;
                    uintptr_t key;
                    ~ScopedErase() { set.erase(key); }
                } loopBodyGuard{ activeLoopBodyNodeIds_, loopKey };

                AppendFlowChainFromOutput(bodyOut->id, bodyScope);
            }
            if (HasError) { delete forNode; return; }

            targetScope->children.push_back(forNode);

            // Execute Completed chain strictly after the loop statement has finished.
            if (const Pin* completedOut = GetOutputPinByName(n, "Completed"))
                AppendFlowChainFromOutput(completedOut->id, targetScope);
            if (HasError) return;

            return;
        }

        case NodeType::While:
        {
            Node* whileNode = BuildWhile(n);
            if (HasError || !whileNode) { delete whileNode; return; }

            Node* bodyScope = (whileNode->children.size() > 1) ? whileNode->children[1] : nullptr;

            if (const Pin* bodyOut = GetOutputPinByName(n, "Body"))
                AppendFlowChainFromOutput(bodyOut->id, bodyScope);
            if (HasError) { delete whileNode; return; }

            targetScope->children.push_back(whileNode);

            if (const Pin* completedOut = GetOutputPinByName(n, "Completed"))
                AppendFlowChainFromOutput(completedOut->id, targetScope);
            if (HasError) return;

            return;
        }

        default:
            break;
    }

    Node* stmt = BuildNode(n);
    if (HasError) { delete stmt; return; }
    if (stmt) targetScope->children.push_back(stmt);

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

// Compile graph to AST

Node* GraphCompiler::Compile(const std::vector<VisualNode>& nodes,
                             const std::vector<Link>&       links)
{
    HasError  = false;
    errorMsg_ = "";
    nodes_ = &nodes;
    activeNodeBuilds_.clear();
    activeLoopBodyNodeIds_.clear();
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

    // Build function body from flow chain starting at Start node.
    Node* body = MakeNode(Token::Scope);

    // 0) Declare variables used by Set Variable nodes once per name.
    std::unordered_set<std::string> declaredVariables;
    for (const VisualNode& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Variable)
            continue;

        const std::string* variant = GetField(n, "Variant");
        const bool isGet = (variant && *variant == "Get");
        if (isGet)
            continue;

        const std::string* nameStr = GetField(n, "Name");
        const std::string varName = (nameStr && !nameStr->empty()) ? *nameStr : "__unnamed";
        if (!declaredVariables.insert(varName).second)
            continue;

        const std::string* typeStr = GetField(n, "Type");
        const std::string typeName = typeStr ? *typeStr : "Number";

        Token typeToken = Token::TypeNumber;
        if (typeName == "Boolean") typeToken = Token::TypeBoolean;
        else if (typeName == "String") typeToken = Token::TypeString;
        else if (typeName == "Array") typeToken = Token::TypeArray;
        else if (typeName == "Any") typeToken = Token::AnyType;

        Node* decl = MakeNode(Token::VarDeclare);
        decl->data = varName;
        decl->children.push_back(MakeNode(typeToken));
        body->children.push_back(decl);
    }

    // 0.1) Declare per-loop "last index" variables once. These retain
    // the previous iteration value after loop completion until loop runs again.
    for (const VisualNode& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Loop)
            continue;

        double startVal = 0.0;
        const Pin* startPin = GetInputPinByName(n, "Start");
        const bool startConnected = (startPin && resolver_.Resolve(startPin->id));
        if (!startConnected)
        {
            const std::string* startStr = GetField(n, "Start");
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
        loopLastDecl->data = LoopLastIndexVarName(n);
        loopLastDecl->children.push_back(MakeNode(Token::TypeNumber));
        loopLastDecl->children.push_back(MakeNumberNode(std::round(startVal)));
        body->children.push_back(loopLastDecl);
    }

    visitedFlowOutputs_.clear();
    activeFlowOutputs_.clear();
    AppendFlowChainFromOutput(startOut->id, body);
    if (HasError) { delete body; return nullptr; }

    // Wrap the body in a function declaration:
    //   def __graph__() { <body> }
    // FunctionDef is consumed by ASTWalker's pre-pass; name goes in data.
    Node* funcDecl = MakeNode(Token::FunctionDef);
    funcDecl->data = std::string(kGraphFunctionName);
    Node* params = MakeNode(Token::CallParams);
    funcDecl->children.push_back(params);
    funcDecl->children.push_back(body);

    // The AST root is a Scope containing the single function declaration.
    Node* root = MakeNode(Token::Scope);
    root->children.push_back(funcDecl);

    return root;
}

Node* GraphCompiler::BuildExpr(const Pin& inputPin)
{
    auto MakeDefaultValue = [&]() -> Node*
    {
        switch (inputPin.type)
        {
            case PinType::Number:  return MakeNumberNode(0.0);
            case PinType::Boolean: return MakeBoolNode(false);
            case PinType::String:  return MakeStringNode("");
            default:               return MakeNode(Token::Null);
        }
    };

    const PinSource* src = resolver_.Resolve(inputPin.id);
    if (!src)
        return MakeDefaultValue();

    if (src->node && NodeRequiresFlow(*src->node))
    {
        const uintptr_t key = static_cast<uintptr_t>(src->node->id.Get());
        if (flowReachableNodes_.find(key) == flowReachableNodes_.end())
            return MakeDefaultValue();
    }

    return BuildNode(*src->node, src->pinIdx);
}

// BuildNode

Node* GraphCompiler::BuildNode(const VisualNode& n, int outPinIdx)
{
    // TODO: outPinIdx is not currently used. This parameter was intended to support
    // nodes with multiple output pins (e.g., Branch has True/False paths). Currently,
    // BuildNode always generates the complete node structure regardless of which
    // specific output is being requested. This works because:
    // 1. Most nodes have only 1 output pin
    // 2. For nodes with multiple outputs (Branch, Function), the structure
    //    (if/else, call) is needed regardless of which output is consumed
    // Future: If a node has multiple independent expressions, this could be used
    // to select which one to return (e.g., return only the True branch's body).
    
    const uintptr_t nodeKey = static_cast<uintptr_t>(n.id.Get());
    if (!activeNodeBuilds_.insert(nodeKey).second)
    {
        Error("Cycle detected while compiling node: " + n.title);
        return nullptr;
    }

    struct ScopedErase {
        std::unordered_set<uintptr_t>& set;
        uintptr_t key;
        ~ScopedErase() { set.erase(key); }
    } guard{ activeNodeBuilds_, nodeKey };

    // Loop has a numeric data output (Index): expose current iteration value
    // when another node reads that output pin as an expression.
    if (n.nodeType == NodeType::Loop
        && outPinIdx >= 0
        && outPinIdx < static_cast<int>(n.outPins.size()))
    {
        const Pin& requestedOutPin = n.outPins[outPinIdx];
        if (requestedOutPin.type != PinType::Flow && requestedOutPin.name == "Index")
        {
            const uintptr_t loopKey = static_cast<uintptr_t>(n.id.Get());
            const bool readingInsideThisLoopBody =
                (activeLoopBodyNodeIds_.find(loopKey) != activeLoopBodyNodeIds_.end());
            return MakeIdNode(readingInsideThisLoopBody
                ? LoopIndexVarName(n)
                : LoopLastIndexVarName(n));
        }
    }

    const NodeDescriptor* descriptor = NodeRegistry::Get().Find(n.nodeType);
    if (descriptor && descriptor->compile)
        return descriptor->compile(this, n);

    Error("No compile callback registered for NodeType");
    return nullptr;
}

// ---------------------------------------------------------------------------
// Internal builder implementations (called by descriptor lambdas in nodeRegistry)
// ---------------------------------------------------------------------------

Node* GraphCompiler::BuildConstant(const VisualNode& n)
{
    const std::string* val = GetField(n, "Value");
    const std::string* type = GetField(n, "Type");

    const std::string value = val ? *val : "";
    const std::string valueType = type ? *type : "";

    // Prefer explicit Constant type over value auto-detection so
    // Boolean false never degrades to Number 0 in AST printout/runtime.
    if (valueType == "Boolean")
    {
        const bool b = (value == "true" || value == "True" || value == "1");
        return MakeBoolNode(b);
    }

    if (valueType == "Number")
    {
        try { return MakeNumberNode(std::stod(value)); }
        catch (...) { return MakeNumberNode(0.0); }
    }

    if (valueType == "String" || valueType == "Array")
        return MakeStringNode(value);

    // Backward compatibility for old/incomplete data without Type field.
    try { return MakeNumberNode(std::stod(value)); }
    catch (...) {}

    if (value == "true"  || value == "1") return MakeBoolNode(true);
    if (value == "false" || value == "0") return MakeBoolNode(false);

    return MakeStringNode(value);
}

Node* GraphCompiler::BuildStart(const VisualNode& n)
{
    // Start is currently an editor-side flow entry marker.
    // Compiler is expression/sink-driven, so this compiles to an empty scope.
    (void)n;
    return MakeNode(Token::Scope);
}

Node* GraphCompiler::BuildOperator(const VisualNode& n)
{
    const Pin* pinA = GetInputPinByName(n, "A");
    const Pin* pinB = GetInputPinByName(n, "B");
    if (!pinA || !pinB) { Error("Operator node needs A and B inputs"); return nullptr; }

    const std::string* opStr = GetField(n, "Op");
    Token tok = opStr ? OperatorToken(*opStr) : Token::Add;
    if (tok == Token::None) { Error("Unknown operator: " + (opStr ? *opStr : "?")); return nullptr; }

    auto buildNumberInputOrDefault = [&](const Pin& pin, const char* fieldName) -> Node*
    {
        const PinSource* src = resolver_.Resolve(pin.id);
        if (src)
            return BuildNode(*src->node, src->pinIdx);

        const std::string* fieldValue = GetField(n, fieldName);
        try { return MakeNumberNode(fieldValue ? std::stod(*fieldValue) : 0.0); }
        catch (...) { return MakeNumberNode(0.0); }
    };

    Node* root = MakeNode(tok);
    Node* lhs  = buildNumberInputOrDefault(*pinA, "A");
    Node* rhs  = buildNumberInputOrDefault(*pinB, "B");
    if (HasError) { delete root; return nullptr; }

    root->children.push_back(lhs);
    root->children.push_back(rhs);
    return root;
}

Node* GraphCompiler::BuildComparison(const VisualNode& n)
{
    const Pin* pinA = GetInputPinByName(n, "A");
    const Pin* pinB = GetInputPinByName(n, "B");
    if (!pinA || !pinB) { Error("Comparison node needs A and B inputs"); return nullptr; }

    const std::string* opStr = GetField(n, "Op");
    Token tok = opStr ? CompareToken(*opStr) : Token::Equal;
    if (tok == Token::None) { Error("Unknown comparison: " + (opStr ? *opStr : "?")); return nullptr; }

    auto buildNumberInputOrDefault = [&](const Pin& pin, const char* fieldName) -> Node*
    {
        const PinSource* src = resolver_.Resolve(pin.id);
        if (src)
            return BuildNode(*src->node, src->pinIdx);

        const std::string* fieldValue = GetField(n, fieldName);
        try { return MakeNumberNode(fieldValue ? std::stod(*fieldValue) : 0.0); }
        catch (...) { return MakeNumberNode(0.0); }
    };

    Node* root = MakeNode(tok);
    Node* lhs  = buildNumberInputOrDefault(*pinA, "A");
    Node* rhs  = buildNumberInputOrDefault(*pinB, "B");
    if (HasError) { delete root; return nullptr; }

    root->children.push_back(lhs);
    root->children.push_back(rhs);
    return root;
}

Node* GraphCompiler::BuildLogic(const VisualNode& n)
{
    const std::string* opStr = GetField(n, "Op");
    Token tok = opStr ? LogicToken(*opStr) : Token::And;
    if (tok == Token::None) { Error("Unknown logic op: " + (opStr ? *opStr : "?")); return nullptr; }

    auto buildBoolInputOrDefault = [&](const Pin& pin, const char* fieldName) -> Node*
    {
        const PinSource* src = resolver_.Resolve(pin.id);
        if (src)
            return BuildNode(*src->node, src->pinIdx);

        const std::string* fieldValue = GetField(n, fieldName);
        const std::string value = fieldValue ? *fieldValue : "false";
        return MakeBoolNode(value == "true" || value == "True" || value == "1");
    };

    Node* root = MakeNode(tok);

    const Pin* pinA = GetInputPinByName(n, "A");
    const Pin* pinB = GetInputPinByName(n, "B");
    if (!pinA || !pinB) { Error("Logic node needs A and B inputs"); return nullptr; }
    Node* lhs = buildBoolInputOrDefault(*pinA, "A");
    Node* rhs = buildBoolInputOrDefault(*pinB, "B");
    if (HasError) { delete root; return nullptr; }
    root->children.push_back(lhs);
    root->children.push_back(rhs);

    return root;
}

Node* GraphCompiler::BuildNot(const VisualNode& n)
{
    const Pin* pinA = GetInputPinByName(n, "A");
    if (!pinA) { Error("Not node needs A input"); return nullptr; }

    auto buildBoolInputOrDefault = [&](const Pin& pin, const char* fieldName) -> Node*
    {
        const PinSource* src = resolver_.Resolve(pin.id);
        if (src)
            return BuildNode(*src->node, src->pinIdx);

        const std::string* fieldValue = GetField(n, fieldName);
        const std::string value = fieldValue ? *fieldValue : "false";
        return MakeBoolNode(value == "true" || value == "True" || value == "1");
    };

    Node* root = MakeNode(Token::Not);
    Node* operand = buildBoolInputOrDefault(*pinA, "A");
    if (HasError) { delete root; return nullptr; }
    root->children.push_back(operand);

    return root;
}

Node* GraphCompiler::BuildDrawRect(const VisualNode& n)
{
    (void)n;
    return MakeNode(Token::Scope);
}

Node* GraphCompiler::BuildDrawGrid(const VisualNode& n)
{
    (void)n;
    return MakeNode(Token::Scope);
}

Node* GraphCompiler::BuildDelay(const VisualNode& n)
{
    (void)n;
    return MakeNode(Token::Scope);
}

Node* GraphCompiler::BuildSequence(const VisualNode& n)
{
    // Sequence is a flow-structuring node in the editor.
    // Current compiler pipeline is expression/sink-driven, so for now
    // compile this as an empty scope placeholder.
    (void)n;
    return MakeNode(Token::Scope);
}

Node* GraphCompiler::BuildBranch(const VisualNode& n)
{
    if (n.inPins.size() < 2) { Error("Branch node needs Flow + Condition inputs"); return nullptr; }

    Node* ifNode   = MakeNode(Token::If);
    Node* condExpr = BuildExpr(n.inPins[1]);
    if (HasError) { delete ifNode; return nullptr; }
    ifNode->children.push_back(condExpr);

    Node* trueScope = MakeNode(Token::Scope);
    ifNode->children.push_back(trueScope);

    if (n.outPins.size() > 1)
    {
        Node* elseScope = MakeNode(Token::Scope);
        Node* elseNode  = MakeNode(Token::Else);
        elseNode->children.push_back(elseScope);
        ifNode->children.push_back(elseNode);
    }

    return ifNode;
}

Node* GraphCompiler::BuildLoop(const VisualNode& n)
{
    const Pin* startPin = GetInputPinByName(n, "Start");
    const Pin* countPin = GetInputPinByName(n, "Count");
    if (!countPin) { Error("Loop node needs Count input"); return nullptr; }

    Node* startExpr = nullptr;
    if (startPin)
    {
        const PinSource* startSrc = resolver_.Resolve(startPin->id);
        if (startSrc)
            startExpr = BuildNode(*startSrc->node, startSrc->pinIdx);
    }

    if (!startExpr)
    {
        const std::string* startStr = GetField(n, "Start");
        double startVal = 0.0;
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
        startExpr = MakeNumberNode(std::round(startVal));
    }
    else if (startExpr->type == Token::Number)
    {
        if (double* v = std::get_if<double>(&startExpr->data))
            *v = std::round(*v);
    }

    Node* countEx = nullptr;
    const PinSource* countSrc = resolver_.Resolve(countPin->id);
    if (countSrc)
    {
        countEx = BuildNode(*countSrc->node, countSrc->pinIdx);
        if (countEx && countEx->type == Token::Number)
        {
            if (double* v = std::get_if<double>(&countEx->data))
                *v = std::round(*v);
        }
    }
    else
    {
        const std::string* countStr = GetField(n, "Count");
        double countVal = 0.0;
        if (countStr)
        {
            try
            {
                countVal = std::stod(*countStr);
            }
            catch (...)
            {
                countVal = 0.0;
            }
        }
        countEx = MakeNumberNode(std::round(countVal));
    }

    if (HasError) { delete startExpr; delete countEx; return nullptr; }

    const std::string indexVarName = LoopIndexVarName(n);
    const std::string lastIndexVarName = LoopLastIndexVarName(n);

    Node* varDecl = MakeNode(Token::VarDeclare);
    varDecl->data = indexVarName;
    Node* initType = MakeNode(Token::TypeNumber);
    varDecl->children.push_back(initType);
    varDecl->children.push_back(startExpr);

    Node* cond    = MakeNode(Token::Less);
    Node* iRef    = MakeIdNode(indexVarName);

    Node* inclusiveUpperBoundExpr = MakeNode(Token::Add);
    inclusiveUpperBoundExpr->children.push_back(countEx);
    inclusiveUpperBoundExpr->children.push_back(MakeNumberNode(1.0));

    cond->children.push_back(iRef);
    cond->children.push_back(inclusiveUpperBoundExpr);

    Node* incr  = MakeNode(Token::Increment);
    Node* iRef2 = MakeIdNode(indexVarName);
    incr->children.push_back(iRef2);

    Node* body = MakeNode(Token::Scope);
    Node* cacheLastIndexAssign = MakeNode(Token::Assign);
    cacheLastIndexAssign->children.push_back(MakeIdNode(lastIndexVarName));
    cacheLastIndexAssign->children.push_back(MakeIdNode(indexVarName));
    body->children.push_back(cacheLastIndexAssign);

    Node* completed = MakeNode(Token::Scope);
    Node* forNode = MakeNode(Token::For);
    forNode->children.push_back(varDecl);
    forNode->children.push_back(cond);
    forNode->children.push_back(incr);
    forNode->children.push_back(body);
    forNode->children.push_back(completed);

    return forNode;
}

Node* GraphCompiler::BuildVariable(const VisualNode& n)
{
    const std::string* nameStr = GetField(n, "Name");
    const std::string* typeStr = GetField(n, "Type");
    const std::string* defaultStr = GetField(n, "Default");

    std::string varName = nameStr ? *nameStr : "__unnamed";

    const PinType defaultType = VariableTypeFromString(typeStr ? *typeStr : "Number");
    const std::string defaultValue = defaultStr ? *defaultStr : "0.0";

    const Pin* setInput = GetInputPinByName(n, "Default");
    if (!setInput)
        setInput = GetInputPinByName(n, "Set"); // backward compatibility
    if (!setInput)
        return MakeIdNode(varName);

    if (setInput)
    {
        const PinSource* src = resolver_.Resolve(setInput->id);
        if (src)
        {
            Node* assign = MakeNode(Token::Assign);
            Node* lhs    = MakeIdNode(varName);
            Node* rhs    = BuildNode(*src->node, src->pinIdx);
            if (HasError) { delete assign; return nullptr; }
            assign->children.push_back(lhs);
            assign->children.push_back(rhs);
            return assign;
        }
    }

    Node* assign = MakeNode(Token::Assign);
    Node* lhs    = MakeIdNode(varName);
    Node* rhs    = nullptr;

    switch (defaultType)
    {
        case PinType::Boolean:
        {
            const bool b = (defaultValue == "true" || defaultValue == "True" || defaultValue == "1");
            rhs = MakeBoolNode(b);
            break;
        }
        case PinType::String:
        case PinType::Array:
            rhs = MakeStringNode(defaultValue);
            break;
        case PinType::Number:
        default:
            try { rhs = MakeNumberNode(std::stod(defaultValue)); }
            catch (...) { rhs = MakeNumberNode(0.0); }
            break;
    }

    assign->children.push_back(lhs);
    assign->children.push_back(rhs);
    return assign;
}

Node* GraphCompiler::BuildWhile(const VisualNode& n)
{
    const Pin* conditionPin = GetInputPinByName(n, "Condition");
    if (!conditionPin)
    {
        Error("While node needs Condition input");
        return nullptr;
    }

    Node* condExpr = BuildExpr(*conditionPin);
    if (HasError || !condExpr)
        return nullptr;

    Node* body = MakeNode(Token::Scope);

    Node* whileNode = MakeNode(Token::While);
    whileNode->children.push_back(condExpr);
    whileNode->children.push_back(body);
    return whileNode;
}

Node* GraphCompiler::BuildOutput(const VisualNode& n)
{
    // Debug Print (NodeType::Output) acts as a flow-triggered print node.
    // It prints the configured label first, then the incoming value.
    Node* scope = MakeNode(Token::Scope);

    const std::string* labelStr = GetField(n, "Label");
    const std::string label = (labelStr && !labelStr->empty()) ? *labelStr : "result";

    Node* printLabel = MakeNode(Token::FunctionCall);
    printLabel->children.push_back(MakeIdNode("println"));
    Node* labelParams = MakeNode(Token::CallParams);
    labelParams->children.push_back(MakeStringNode("[Debug Print] " + label));
    printLabel->children.push_back(labelParams);
    scope->children.push_back(printLabel);

    const Pin* valuePin = GetInputPinByName(n, "Value");
    if (valuePin)
    {
        Node* printValue = MakeNode(Token::FunctionCall);
        printValue->children.push_back(MakeIdNode("println"));
        Node* valueParams = MakeNode(Token::CallParams);
        valueParams->children.push_back(BuildExpr(*valuePin));
        if (HasError) { delete scope; return nullptr; }
        printValue->children.push_back(valueParams);
        scope->children.push_back(printValue);
    }

    return scope;
}

Node* GraphCompiler::BuildFunction(const VisualNode& n)
{
    const std::string* nameStr = GetField(n, "Name");
    std::string funcName = nameStr ? *nameStr : "__fn";

    Node* call   = MakeNode(Token::FunctionCall);
    Node* nameId = MakeIdNode(funcName);
    call->children.push_back(nameId);

    Node* params = MakeNode(Token::CallParams);

    for (const Pin& pin : n.inPins)
    {
        if (pin.type == PinType::Flow) continue;
        Node* arg = BuildExpr(pin);
        if (HasError) { delete call; return nullptr; }
        params->children.push_back(arg);
    }

    call->children.push_back(params);

    return call;
}

