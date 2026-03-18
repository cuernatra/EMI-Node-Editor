#include "graphCompiler.h"
#include "../registry/nodeRegistry.h"
#include <algorithm>
#include <stdexcept>
#include <cassert>
#include <memory>


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
    if (op == "NOT") return Token::Not;
    return Token::None;
}

static PinType VariableTypeFromString(const std::string& typeName)
{
    if (typeName == "Boolean") return PinType::Boolean;
    if (typeName == "String")  return PinType::String;
    if (typeName == "Array")   return PinType::Array;
    return PinType::Number;
}

const Pin* GraphCompiler::GetOutputPinByName(const VisualNode& n, const char* name) const
{
    for (const Pin& p : n.outPins)
        if (p.name == name)
            return &p;
    return nullptr;
}

const VisualNode* GraphCompiler::FindFlowTargetNode(const Pin& outFlowPin) const
{
    if (!links_)
        return nullptr;

    for (const Link& link : *links_)
    {
        if (!link.alive || link.startPinId != outFlowPin.id)
            continue;

        auto it = pinsById_.find(link.endPinId);
        if (it == pinsById_.end() || !it->second)
            continue;

        const Pin* endPin = it->second;
        if (!endPin->isInput || endPin->type != PinType::Flow)
            continue;

        return resolver_.FindNode(endPin->parentNodeId);
    }

    return nullptr;
}

Node* GraphCompiler::BuildFlowChainFromPin(const Pin& outFlowPin)
{
    if (outFlowPin.type != PinType::Flow || outFlowPin.isInput)
        return nullptr;

    const VisualNode* target = FindFlowTargetNode(outFlowPin);
    if (!target)
        return nullptr;

    return BuildFlowChainFromNode(*target);
}

Node* GraphCompiler::BuildFlowChainFromNode(const VisualNode& node)
{
    Node* current = BuildNode(node);
    if (HasError || !current)
        return current;

    const bool autoContinue =
        node.nodeType == NodeType::FunctionCall ||
        node.nodeType == NodeType::Function ||
        node.nodeType == NodeType::Variable;

    if (!autoContinue)
        return current;

    const Pin* out = GetOutputPinByName(node, "Out");
    if (!out || out->type != PinType::Flow)
        return current;

    Node* next = BuildFlowChainFromPin(*out);
    if (HasError || !next)
        return current;

    Node* scope = MakeNode(Token::Scope);
    scope->children.push_back(current);
    scope->children.push_back(next);
    return scope;
}

bool GraphCompiler::HasIncomingFlow(const VisualNode& n) const
{
    if (!links_)
        return false;

    for (const Pin& p : n.inPins)
    {
        if (p.type != PinType::Flow)
            continue;

        for (const Link& link : *links_)
        {
            if (link.alive && link.endPinId == p.id)
                return true;
        }
    }

    return false;
}

// Compile graph to AST

Node* GraphCompiler::Compile(const std::vector<VisualNode>& nodes,
                             const std::vector<Link>&       links)
{
    HasError  = false;
    errorMsg_ = "";
    activeNodeBuilds_.clear();

    resolver_.Build(nodes, links);
    links_ = &links;
    pinsById_.clear();
    for (const VisualNode& n : nodes)
    {
        if (!n.alive) continue;
        for (const Pin& p : n.inPins)  pinsById_[p.id] = &p;
        for (const Pin& p : n.outPins) pinsById_[p.id] = &p;
    }

    // Build a Scope containing statements from explicit entry points.
    auto body = std::unique_ptr<Node>(MakeNode(Token::Scope));

    auto isFunctionCallResultConsumed = [&](const VisualNode& node) -> bool
    {
        if (node.nodeType != NodeType::FunctionCall)
            return false;

        ed::PinId resultPin;
        bool hasResultPin = false;
        for (const Pin& outPin : node.outPins)
        {
            if (outPin.name == "Result")
            {
                resultPin = outPin.id;
                hasResultPin = true;
                break;
            }
        }

        if (!hasResultPin)
            return false;

        for (const Link& link : links)
        {
            if (link.alive && link.startPinId == resultPin)
                return true;
        }

        return false;
    };

    // Track visited nodes during execution traversal to avoid duplicates.
    std::unordered_set<uintptr_t> visitedNodes;

    // Phase 1: Emit statements from explicit flow entry points (Start nodes without incoming flow).
    for (const VisualNode& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Start)
            continue;

        if (HasIncomingFlow(n))
            continue;  // Skip Start nodes with incoming flow; they are not entry points.

        Node* stmt = BuildFlowChainFromNode(n);
        if (HasError) { return nullptr; }
        if (stmt) body->children.push_back(stmt);

        visitedNodes.insert(static_cast<uintptr_t>(n.id.Get()));
    }

    // Phase 2: Handle isolated sinks (Output, Function, unconsumed FunctionCall) with no incoming flow.
    // These are fallback entry points for graphs with no Start node.
    for (const VisualNode& n : nodes)
    {
        if (!n.alive) continue;

        // Skip if already visited via flow traversal.
        if (visitedNodes.count(static_cast<uintptr_t>(n.id.Get())))
            continue;

        const bool isIsolatedSink =
            !HasIncomingFlow(n) &&
            (n.nodeType == NodeType::Output ||
             n.nodeType == NodeType::Function ||
             (n.nodeType == NodeType::FunctionCall && !isFunctionCallResultConsumed(n)));

        if (isIsolatedSink)
        {
            Node* stmt = BuildNode(n);
            if (HasError) { return nullptr; }
            if (stmt) body->children.push_back(stmt);

            visitedNodes.insert(static_cast<uintptr_t>(n.id.Get()));
        }
    }

    // Wrap the body in a function declaration:
    //   def __graph__() { <body> }
    // FunctionDef is consumed by ASTWalker's pre-pass; name goes in data.
    auto funcDecl = std::unique_ptr<Node>(MakeNode(Token::FunctionDef));
    funcDecl->data = std::string(kGraphFunctionName);
    auto params = std::unique_ptr<Node>(MakeNode(Token::CallParams));
    funcDecl->children.push_back(params.release());
    funcDecl->children.push_back(body.release());

    // The AST root is a Scope containing the single function declaration.
    auto root = std::unique_ptr<Node>(MakeNode(Token::Scope));
    root->children.push_back(funcDecl.release());

    return root.release();
}

Node* GraphCompiler::BuildExpr(const Pin& inputPin)
{
    const PinSource* src = resolver_.Resolve(inputPin.id);
    if (!src)
    {
        switch (inputPin.type)
        {
            case PinType::Number:  return MakeNumberNode(0.0);
            case PinType::Boolean: return MakeBoolNode(false);
            case PinType::String:  return MakeStringNode("");
            default:               return MakeNode(Token::Null);
        }
    }
    return BuildNode(*src->node, src->pinIdx);
}

// BuildNode

Node* GraphCompiler::BuildNode(const VisualNode& n, int /*outPinIdx*/)
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
    const Pin* execOut = GetOutputPinByName(n, "Exec");
    if (!execOut)
        return MakeNode(Token::Scope);

    Node* chain = BuildFlowChainFromPin(*execOut);
    if (HasError)
        return nullptr;
    if (!chain)
        return MakeNode(Token::Scope);

    return chain;
}

Node* GraphCompiler::BuildOperator(const VisualNode& n)
{
    const Pin* pinA = GetInputPinByName(n, "A");
    const Pin* pinB = GetInputPinByName(n, "B");
    if (!pinA || !pinB) { Error("Operator node needs A and B inputs"); return nullptr; }

    const std::string* opStr = GetField(n, "Op");
    Token tok = opStr ? OperatorToken(*opStr) : Token::Add;
    if (tok == Token::None) { Error("Unknown operator: " + (opStr ? *opStr : "?")); return nullptr; }

    Node* root = MakeNode(tok);
    Node* lhs  = BuildExpr(*pinA);
    Node* rhs  = BuildExpr(*pinB);
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

    Node* root = MakeNode(tok);
    Node* lhs  = BuildExpr(*pinA);
    Node* rhs  = BuildExpr(*pinB);
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

    Node* root = MakeNode(tok);

    if (tok == Token::Not)
    {
        const Pin* pinA = GetInputPinByName(n, "A");
        if (!pinA) { Error("NOT node needs A input"); return nullptr; }
        Node* operand = BuildExpr(*pinA);
        if (HasError) { delete root; return nullptr; }
        root->children.push_back(operand);
    }
    else
    {
        const Pin* pinA = GetInputPinByName(n, "A");
        const Pin* pinB = GetInputPinByName(n, "B");
        if (!pinA || !pinB) { Error("Logic node needs A and B inputs"); return nullptr; }
        Node* lhs = BuildExpr(*pinA);
        Node* rhs = BuildExpr(*pinB);
        if (HasError) { delete root; return nullptr; }
        root->children.push_back(lhs);
        root->children.push_back(rhs);
    }

    return root;
}

Node* GraphCompiler::BuildSequence(const VisualNode& n)
{
    Node* scope = MakeNode(Token::Scope);

    // Emit Then outputs in numeric order: Then 0, Then 1, ...
    std::vector<const Pin*> thenPins;
    thenPins.reserve(n.outPins.size());
    for (const Pin& p : n.outPins)
    {
        if (p.type == PinType::Flow && !p.isInput && p.name.rfind("Then ", 0) == 0)
            thenPins.push_back(&p);
    }

    std::sort(thenPins.begin(), thenPins.end(), [](const Pin* a, const Pin* b)
    {
        auto parseIndex = [](const std::string& s) -> int
        {
            if (s.size() <= 5) return 0;
            try { return std::stoi(s.substr(5)); }
            catch (...) { return 0; }
        };
        return parseIndex(a->name) < parseIndex(b->name);
    });

    for (const Pin* thenPin : thenPins)
    {
        Node* stmt = BuildFlowChainFromPin(*thenPin);
        if (HasError) { delete scope; return nullptr; }
        if (stmt) scope->children.push_back(stmt);
    }

    return scope;
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

    if (const Pin* trueOut = GetOutputPinByName(n, "True"))
    {
        Node* trueStmt = BuildFlowChainFromPin(*trueOut);
        if (HasError) { delete ifNode; return nullptr; }
        if (trueStmt)
            trueScope->children.push_back(trueStmt);
    }

    if (n.outPins.size() > 1)
    {
        Node* elseScope = MakeNode(Token::Scope);
        if (const Pin* falseOut = GetOutputPinByName(n, "False"))
        {
            Node* falseStmt = BuildFlowChainFromPin(*falseOut);
            if (HasError) { delete ifNode; return nullptr; }
            if (falseStmt)
                elseScope->children.push_back(falseStmt);
        }
        Node* elseNode  = MakeNode(Token::Else);
        elseNode->children.push_back(elseScope);
        ifNode->children.push_back(elseNode);
    }

    return ifNode;
}

Node* GraphCompiler::BuildLoop(const VisualNode& n)
{
    if (n.inPins.size() < 2) { Error("Loop node needs Flow + Count inputs"); return nullptr; }

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

    Node* varDecl = MakeNode(Token::VarDeclare);
    Node* iId     = MakeIdNode("__i");
    Node* initVal = MakeNumberNode(startVal);
    varDecl->children.push_back(iId);
    varDecl->children.push_back(initVal);

    Node* cond    = MakeNode(Token::Less);
    Node* iRef    = MakeIdNode("__i");
    Node* countEx = BuildExpr(n.inPins[1]);
    if (HasError) { delete varDecl; delete cond; return nullptr; }
    cond->children.push_back(iRef);
    cond->children.push_back(countEx);

    Node* incr  = MakeNode(Token::Increment);
    Node* iRef2 = MakeIdNode("__i");
    incr->children.push_back(iRef2);

    Node* body    = MakeNode(Token::Scope);
    Node* forNode = MakeNode(Token::For);
    forNode->children.push_back(varDecl);
    forNode->children.push_back(cond);
    forNode->children.push_back(incr);
    forNode->children.push_back(body);

    if (const Pin* bodyOut = GetOutputPinByName(n, "Body"))
    {
        Node* bodyStmt = BuildFlowChainFromPin(*bodyOut);
        if (HasError) { delete forNode; return nullptr; }
        if (bodyStmt)
            body->children.push_back(bodyStmt);
    }

    Node* completedStmt = nullptr;
    if (const Pin* completedOut = GetOutputPinByName(n, "Completed"))
    {
        completedStmt = BuildFlowChainFromPin(*completedOut);
        if (HasError) { delete forNode; return nullptr; }
    }

    if (!completedStmt)
        return forNode;

    Node* scope = MakeNode(Token::Scope);
    scope->children.push_back(forNode);
    scope->children.push_back(completedStmt);
    return scope;
}

Node* GraphCompiler::BuildVariable(const VisualNode& n)
{
    const std::string* nameStr = GetField(n, "Name");
    std::string varName = nameStr ? *nameStr : "__unnamed";

    if (!n.inPins.empty())
    {
        const PinSource* src = resolver_.Resolve(n.inPins[0].id);
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

    return MakeIdNode(varName);
}

Node* GraphCompiler::BuildOutput(const VisualNode& n)
{
    // Output emits a return statement so the function hands the value back
    // to the caller via VM::GetReturnValue().
    Node* ret = MakeNode(Token::Return);

    if (!n.inPins.empty())
    {
        Node* val = BuildExpr(n.inPins[0]);
        if (HasError) { delete ret; return nullptr; }
        ret->children.push_back(val);
    }

    return ret;
}

Node* GraphCompiler::BuildFunction(const VisualNode& n)
{
    const std::string* nameStr = GetField(n, "Name");
    std::string funcName = nameStr ? *nameStr : "__fn";

    Node* call   = MakeNode(Token::FunctionCall);
    Node* nameId = MakeIdNode(funcName);
    Node* params = MakeNode(Token::CallParams);
    call->children.push_back(nameId);
    call->children.push_back(params);

    for (const Pin& pin : n.inPins)
    {
        if (pin.type == PinType::Flow) continue;
        Node* arg = BuildExpr(pin);
        if (HasError) { delete call; return nullptr; }
        params->children.push_back(arg);
    }

    return call;
}

Node* GraphCompiler::BuildFunctionCall(const VisualNode& n)
{
    const std::string* nameStr = GetField(n, "Name");
    std::string funcName = nameStr ? *nameStr : "__fn";

    Node* call   = MakeNode(Token::FunctionCall);
    Node* nameId = MakeIdNode(funcName);
    Node* params = MakeNode(Token::CallParams);
    call->children.push_back(nameId);
    call->children.push_back(params);

    // All non-Flow input pins (Arg0, Arg1, ...) become positional arguments.
    for (const Pin& pin : n.inPins)
    {
        if (pin.type == PinType::Flow) continue;
        Node* arg = BuildExpr(pin);
        if (HasError) { delete call; return nullptr; }
        params->children.push_back(arg);
    }

    return call;
}

