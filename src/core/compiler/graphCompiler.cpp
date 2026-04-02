#include "graphCompiler.h"
#include "../registry/nodeRegistry.h"
#include <stdexcept>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>


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

static std::string TrimCopy(const std::string& s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

static bool TryParseDoubleExact(const std::string& s, double& out)
{
    if (s.empty())
        return false;

    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end && *end == '\0')
    {
        out = v;
        return true;
    }

    return false;
}

static std::vector<std::string> SplitArrayItemsTopLevel(const std::string& text)
{
    std::vector<std::string> out;
    std::string current;
    current.reserve(text.size());

    int bracketDepth = 0;
    int parenDepth = 0;
    int braceDepth = 0;
    bool inQuote = false;
    char quoteChar = '\0';
    bool escape = false;

    auto pushCurrent = [&]() {
        out.push_back(TrimCopy(current));
        current.clear();
    };

    for (char ch : text)
    {
        if (escape)
        {
            current.push_back(ch);
            escape = false;
            continue;
        }

        if (inQuote)
        {
            current.push_back(ch);
            if (ch == '\\')
            {
                escape = true;
            }
            else if (ch == quoteChar)
            {
                inQuote = false;
            }
            continue;
        }

        if (ch == '"' || ch == '\'')
        {
            inQuote = true;
            quoteChar = ch;
            current.push_back(ch);
            continue;
        }

        if (ch == '[') ++bracketDepth;
        else if (ch == ']') bracketDepth = std::max(0, bracketDepth - 1);
        else if (ch == '(') ++parenDepth;
        else if (ch == ')') parenDepth = std::max(0, parenDepth - 1);
        else if (ch == '{') ++braceDepth;
        else if (ch == '}') braceDepth = std::max(0, braceDepth - 1);

        if (ch == ',' && bracketDepth == 0 && parenDepth == 0 && braceDepth == 0)
        {
            pushCurrent();
            continue;
        }

        current.push_back(ch);
    }

    pushCurrent();
    return out;
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

std::string GraphCompiler::LoopStartVarName(const VisualNode& n) const
{
    return "__loop_start_i_" + std::to_string(static_cast<unsigned long long>(static_cast<uintptr_t>(n.id.Get())));
}

std::string GraphCompiler::LoopEndVarName(const VisualNode& n) const
{
    return "__loop_end_i_" + std::to_string(static_cast<unsigned long long>(static_cast<uintptr_t>(n.id.Get())));
}

std::string GraphCompiler::ForEachIterVarName(const VisualNode& n) const
{
    return "__foreach_it_" + std::to_string(static_cast<unsigned long long>(static_cast<uintptr_t>(n.id.Get())));
}

std::string GraphCompiler::ForEachElementVarName(const VisualNode& n) const
{
    return "__foreach_elem_" + std::to_string(static_cast<unsigned long long>(static_cast<uintptr_t>(n.id.Get())));
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

    // Function-node käännetään erikseen Compile()-loopin kautta!!!!!!!!!!!!!!!=??????????????
    // joten ohitetaan se flow-ketjussa
    if (n.nodeType == NodeType::Function)
    {
        if (const Pin* outFlow = GetOutputPinByName(n, "Out"))
            AppendFlowChainFromOutput(outFlow->id, targetScope);
        return;
    }
    //?????????????????????????????????????????????????????????????????????????????????????????????

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

            Node* bodyScope = nullptr;
            if (forNode->type == Token::For)
            {
                bodyScope = (forNode->children.size() > 3) ? forNode->children[3] : nullptr;
            }
            else if (forNode->type == Token::Scope)
            {
                // Loop builder currently returns a prelude scope that contains
                // start/end temporary declarations and the actual For node as
                // its last child. Body flow must be appended inside that inner
                // For body scope so it executes on every iteration.
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

        case NodeType::ForEach:
        {
            Node* forNode = BuildForEach(n);
            if (HasError || !forNode) { delete forNode; return; }

            Node* bodyScope = (forNode->children.size() > 1) ? forNode->children[1] : nullptr;

            if (const Pin* bodyOut = GetOutputPinByName(n, "Body"))
            {
                const uintptr_t forEachKey = static_cast<uintptr_t>(n.id.Get());
                activeForEachBodyNodeIds_.insert(forEachKey);
                struct ScopedErase {
                    std::unordered_set<uintptr_t>& set;
                    uintptr_t key;
                    ~ScopedErase() { set.erase(key); }
                } forEachGuard{ activeForEachBodyNodeIds_, forEachKey };

                AppendFlowChainFromOutput(bodyOut->id, bodyScope);
            }
            if (HasError) { delete forNode; return; }

            targetScope->children.push_back(forNode);

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

    // 0.2) Declare temp variables for CallFunction result pins?????????????????????????????????
    for (const VisualNode& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::CallFunction)
            continue;

        const std::string tempVar = "__call_result_" +
            std::to_string(static_cast<uintptr_t>(n.id.Get()));

        Node* decl = MakeNode(Token::VarDeclare);
        decl->data = tempVar;
        decl->children.push_back(MakeNode(Token::AnyType));
        body->children.push_back(decl);
    }

    visitedFlowOutputs_.clear();
    activeFlowOutputs_.clear();
    AppendFlowChainFromOutput(startOut->id, body);
    if (HasError) { delete body; return nullptr; }

    // Wrap the body in a function declaration:
    Node* funcDecl = MakeNode(Token::FunctionDef);
    funcDecl->data = std::string(kGraphFunctionName);
    Node* params = MakeNode(Token::CallParams);
    funcDecl->children.push_back(params);
    funcDecl->children.push_back(body);

    // Kerää Function-nodet ensin
    std::vector<Node*> userFunctions;
    for (const VisualNode& n : nodes)
    {
        if (!n.alive || n.nodeType != NodeType::Function)
            continue;

        Node* userFunc = BuildFunction(n);
        if (HasError)
        {
            for (Node* f : userFunctions) delete f;
            delete body;
            delete funcDecl;
            return nullptr;
        }
        if (userFunc) userFunctions.push_back(userFunc);
    }

    Node* root = MakeNode(Token::Scope);
    for (Node* f : userFunctions)
        root->children.push_back(f);
    root->children.push_back(funcDecl);

    return root;//??????????????????????????????????????????????????????
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

    // CallFunction-noden Result-pini on data eikä flow
    // joten ohitetaan flow-reachable tarkistus?????????????????????????????????????????????????????????????????
    const bool isCallFunction = (src->node && src->node->nodeType == NodeType::CallFunction);

    if (!isCallFunction && src->node && NodeRequiresFlow(*src->node))
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

    //????????????????????????????????????????????????????????????????????????????????????????????????????????????
    if (n.nodeType == NodeType::CallFunction
        && outPinIdx >= 0
        && outPinIdx < static_cast<int>(n.outPins.size()))
    {
        const Pin& requestedOutPin = n.outPins[outPinIdx];
        if (requestedOutPin.name == "Result")
        {
            const std::string tempVar = "__call_result_" +
                std::to_string(static_cast<uintptr_t>(n.id.Get()));
            return MakeIdNode(tempVar);
        }
    }

    // ForEach exposes current element while body executes, and last value otherwise.
    if (n.nodeType == NodeType::ForEach
        && outPinIdx >= 0
        && outPinIdx < static_cast<int>(n.outPins.size()))
    {
        const Pin& requestedOutPin = n.outPins[outPinIdx];
        if (requestedOutPin.type != PinType::Flow && requestedOutPin.name == "Element")
        {
            return MakeIdNode(ForEachElementVarName(n));
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

    if (valueType == "String")
        return MakeStringNode(value);

    if (valueType == "Array")
        return BuildArrayLiteralNode(value);

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
    Node* call = MakeNode(Token::FunctionCall);
    call->children.push_back(MakeIdNode("delay"));

    Node* params = MakeNode(Token::CallParams);
    if (const Pin* durationPin = GetInputPinByName(n, "Duration"))
    {
        const PinSource* durationSrc = resolver_.Resolve(durationPin->id);
        if (durationSrc)
        {
            params->children.push_back(BuildNode(*durationSrc->node, durationSrc->pinIdx));
            if (HasError) { delete call; return nullptr; }
        }
        else
        {
            const std::string* durationStr = GetField(n, "Duration");
            double durationMs = 1000.0;
            if (durationStr)
            {
                try
                {
                    durationMs = std::stod(*durationStr);
                }
                catch (...)
                {
                    durationMs = 1000.0;
                }
            }
            params->children.push_back(MakeNumberNode(durationMs));
        }
    }
    else
    {
        params->children.push_back(MakeNumberNode(1000.0));
    }

    call->children.push_back(params);
    return call;
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
    const std::string startVarName = LoopStartVarName(n);
    const std::string endVarName = LoopEndVarName(n);

    Node* prelude = MakeNode(Token::Scope);

    Node* startDecl = MakeNode(Token::VarDeclare);
    startDecl->data = startVarName;
    startDecl->children.push_back(MakeNode(Token::TypeNumber));
    startDecl->children.push_back(startExpr);
    prelude->children.push_back(startDecl);

    Node* endDecl = MakeNode(Token::VarDeclare);
    endDecl->data = endVarName;
    endDecl->children.push_back(MakeNode(Token::TypeNumber));
    Node* endExpr = MakeNode(Token::Add);
    endExpr->children.push_back(MakeIdNode(startVarName));
    endExpr->children.push_back(countEx);
    endDecl->children.push_back(endExpr);
    prelude->children.push_back(endDecl);

    Node* varDecl = MakeNode(Token::VarDeclare);
    varDecl->data = indexVarName;
    Node* initType = MakeNode(Token::TypeNumber);
    varDecl->children.push_back(initType);
    varDecl->children.push_back(MakeIdNode(startVarName));

    Node* cond    = MakeNode(Token::Less);
    Node* iRef    = MakeIdNode(indexVarName);
    cond->children.push_back(iRef);
    cond->children.push_back(MakeIdNode(endVarName));

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

    prelude->children.push_back(forNode);

    return prelude;
}

Node* GraphCompiler::BuildArrayLiteralNode(const std::string& text) const
{
    std::string s = TrimCopy(text);
    if (s.empty())
        return MakeNode(Token::Array);

    if (s.front() == '[' && s.back() == ']')
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

        // Fallback to plain string token.
        arr->children.push_back(MakeStringNode(item));
    }

    return arr;
}

Node* GraphCompiler::BuildForEach(const VisualNode& n)
{
    const Pin* arrayPin = GetInputPinByName(n, "Array");
    if (!arrayPin)
    {
        Error("For Each node needs Array input");
        return nullptr;
    }

    Node* arrayExpr = nullptr;
    if (const PinSource* arraySrc = resolver_.Resolve(arrayPin->id))
    {
        arrayExpr = BuildNode(*arraySrc->node, arraySrc->pinIdx);
    }
    else
    {
        const std::string* arrayText = GetField(n, "Array");
        arrayExpr = BuildArrayLiteralNode(arrayText ? *arrayText : "[]");
    }

    if (HasError || !arrayExpr)
    {
        delete arrayExpr;
        return nullptr;
    }

    Node* loop = MakeNode(Token::For);
    loop->data = ForEachElementVarName(n);

    // EMI range-for shape:
    //   For(data=<varName>) with 3 children:
    //     [0] iterable expression
    //     [1] body scope
    //     [2] completed/else scope
    Node* bodyScope = MakeNode(Token::Scope);
    Node* completedScope = MakeNode(Token::Scope);

    loop->children.push_back(arrayExpr);
    loop->children.push_back(bodyScope);
    loop->children.push_back(completedScope);

    return loop;
}

Node* GraphCompiler::BuildArrayGetAt(const VisualNode& n)
{
    const Pin* arrayPin = GetInputPinByName(n, "Array");
    const Pin* indexPin = GetInputPinByName(n, "Index");
    if (!arrayPin || !indexPin)
    {
        Error("Array Get node needs Array and Index inputs");
        return nullptr;
    }

    Node* arrayExpr = nullptr;
    if (const PinSource* arraySrc = resolver_.Resolve(arrayPin->id))
        arrayExpr = BuildNode(*arraySrc->node, arraySrc->pinIdx);
    else
    {
        const std::string* arrayText = GetField(n, "Array");
        arrayExpr = BuildArrayLiteralNode(arrayText ? *arrayText : "[]");
    }

    Node* indexExpr = nullptr;
    if (const PinSource* indexSrc = resolver_.Resolve(indexPin->id))
        indexExpr = BuildNode(*indexSrc->node, indexSrc->pinIdx);
    else
    {
        const std::string* indexText = GetField(n, "Index");
        double indexValue = 0.0;
        if (indexText)
        {
            try { indexValue = std::stod(*indexText); }
            catch (...) { indexValue = 0.0; }
        }
        indexExpr = MakeNumberNode(indexValue);
    }

    if (HasError || !arrayExpr || !indexExpr)
    {
        delete arrayExpr;
        delete indexExpr;
        return nullptr;
    }

    Node* indexer = MakeNode(Token::Indexer);
    indexer->children.push_back(arrayExpr);
    indexer->children.push_back(indexExpr);
    return indexer;
}

Node* GraphCompiler::BuildArrayAddAt(const VisualNode& n)
{
    const Pin* arrayPin = GetInputPinByName(n, "Array");
    const Pin* indexPin = GetInputPinByName(n, "Index");
    const Pin* valuePin = GetInputPinByName(n, "Value");
    if (!arrayPin || !indexPin || !valuePin)
    {
        Error("Array Add node needs Array, Index and Value inputs");
        return nullptr;
    }

    Node* arrayExpr = nullptr;
    if (const PinSource* arraySrc = resolver_.Resolve(arrayPin->id))
        arrayExpr = BuildNode(*arraySrc->node, arraySrc->pinIdx);
    else
    {
        const std::string* arrayText = GetField(n, "Array");
        arrayExpr = BuildArrayLiteralNode(arrayText ? *arrayText : "[]");
    }

    Node* indexExpr = nullptr;
    if (const PinSource* indexSrc = resolver_.Resolve(indexPin->id))
        indexExpr = BuildNode(*indexSrc->node, indexSrc->pinIdx);
    else
    {
        const std::string* indexText = GetField(n, "Index");
        double indexValue = 0.0;
        if (indexText)
        {
            try { indexValue = std::stod(*indexText); }
            catch (...) { indexValue = 0.0; }
        }
        indexExpr = MakeNumberNode(indexValue);
    }

    Node* valueExpr = BuildExpr(*valuePin);
    if (HasError || !arrayExpr || !indexExpr || !valueExpr)
    {
        delete arrayExpr;
        delete indexExpr;
        delete valueExpr;
        return nullptr;
    }

    Node* call = MakeNode(Token::FunctionCall);
    call->children.push_back(MakeIdNode("Array.Insert"));
    Node* params = MakeNode(Token::CallParams);
    params->children.push_back(arrayExpr);
    params->children.push_back(indexExpr);
    params->children.push_back(valueExpr);
    call->children.push_back(params);
    return call;
}

Node* GraphCompiler::BuildArrayRemoveAt(const VisualNode& n)
{
    const Pin* arrayPin = GetInputPinByName(n, "Array");
    const Pin* indexPin = GetInputPinByName(n, "Index");
    if (!arrayPin || !indexPin)
    {
        Error("Array Remove node needs Array and Index inputs");
        return nullptr;
    }

    Node* arrayExpr = nullptr;
    if (const PinSource* arraySrc = resolver_.Resolve(arrayPin->id))
        arrayExpr = BuildNode(*arraySrc->node, arraySrc->pinIdx);
    else
    {
        const std::string* arrayText = GetField(n, "Array");
        arrayExpr = BuildArrayLiteralNode(arrayText ? *arrayText : "[]");
    }

    Node* indexExpr = nullptr;
    if (const PinSource* indexSrc = resolver_.Resolve(indexPin->id))
        indexExpr = BuildNode(*indexSrc->node, indexSrc->pinIdx);
    else
    {
        const std::string* indexText = GetField(n, "Index");
        double indexValue = 0.0;
        if (indexText)
        {
            try { indexValue = std::stod(*indexText); }
            catch (...) { indexValue = 0.0; }
        }
        indexExpr = MakeNumberNode(indexValue);
    }

    if (HasError || !arrayExpr || !indexExpr)
    {
        delete arrayExpr;
        delete indexExpr;
        return nullptr;
    }

    Node* call = MakeNode(Token::FunctionCall);
    call->children.push_back(MakeIdNode("Array.RemoveIndex"));
    Node* params = MakeNode(Token::CallParams);
    params->children.push_back(arrayExpr);
    params->children.push_back(indexExpr);
    call->children.push_back(params);
    return call;
}

Node* GraphCompiler::BuildGridNodeSchema(const VisualNode& n)
{
    auto fieldOr = [&](const char* name, const std::string& fallback) -> std::string
    {
        if (const std::string* v = GetField(n, name))
            return *v;
        return fallback;
    };

    auto parseNumber = [&](const std::string& s, double fallback) -> double
    {
        try { return std::stod(s); }
        catch (...) { return fallback; }
    };

    auto parseBool = [&](const std::string& s) -> bool
    {
        return (s == "true" || s == "True" || s == "1");
    };

    Node* nodeArray = MakeNode(Token::Array);
    nodeArray->children.push_back(MakeNumberNode(parseNumber(fieldOr("Id", "0"), 0.0)));
    nodeArray->children.push_back(MakeStringNode(fieldOr("Tag", "node_0")));
    nodeArray->children.push_back(MakeStringNode(fieldOr("Type", "Empty")));
    nodeArray->children.push_back(MakeNumberNode(parseNumber(fieldOr("X", "0"), 0.0)));
    nodeArray->children.push_back(MakeNumberNode(parseNumber(fieldOr("Y", "0"), 0.0)));
    nodeArray->children.push_back(MakeNumberNode(parseNumber(fieldOr("R", "120"), 120.0)));
    nodeArray->children.push_back(MakeNumberNode(parseNumber(fieldOr("G", "180"), 180.0)));
    nodeArray->children.push_back(MakeNumberNode(parseNumber(fieldOr("B", "255"), 255.0)));
    nodeArray->children.push_back(MakeBoolNode(parseBool(fieldOr("IsWall", "false"))));
    nodeArray->children.push_back(MakeNumberNode(parseNumber(fieldOr("Distance", "0"), 0.0)));
    nodeArray->children.push_back(BuildArrayLiteralNode(fieldOr("Neighbors", "[]")));

    return nodeArray;
}

Node* GraphCompiler::BuildGridNodeCreate(const VisualNode& n)
{
    auto buildInputOrField = [&](const char* pinName,
                                 const char* fieldName,
                                 PinType expectedType,
                                 const std::string& fallback) -> Node*
    {
        if (const Pin* pin = GetInputPinByName(n, pinName))
        {
            if (const PinSource* src = resolver_.Resolve(pin->id))
                return BuildNode(*src->node, src->pinIdx);
        }

        std::string value = fallback;
        if (const std::string* f = GetField(n, fieldName))
            value = *f;

        switch (expectedType)
        {
            case PinType::Number:
                try { return MakeNumberNode(std::stod(value)); }
                catch (...) { return MakeNumberNode(0.0); }
            case PinType::Boolean:
                return MakeBoolNode(value == "true" || value == "True" || value == "1");
            case PinType::String:
                return MakeStringNode(value);
            case PinType::Array:
                return BuildArrayLiteralNode(value);
            default:
                return MakeNode(Token::Null);
        }
    };

    Node* nodeArray = MakeNode(Token::Array);
    nodeArray->children.push_back(buildInputOrField("Id", "Id", PinType::Number, "0"));
    if (HasError) { delete nodeArray; return nullptr; }
    nodeArray->children.push_back(buildInputOrField("Tag", "Tag", PinType::String, "node_0"));
    if (HasError) { delete nodeArray; return nullptr; }
    nodeArray->children.push_back(buildInputOrField("Type", "Type", PinType::String, "Empty"));
    if (HasError) { delete nodeArray; return nullptr; }
    nodeArray->children.push_back(buildInputOrField("X", "X", PinType::Number, "0"));
    if (HasError) { delete nodeArray; return nullptr; }
    nodeArray->children.push_back(buildInputOrField("Y", "Y", PinType::Number, "0"));
    if (HasError) { delete nodeArray; return nullptr; }
    nodeArray->children.push_back(buildInputOrField("R", "R", PinType::Number, "120"));
    if (HasError) { delete nodeArray; return nullptr; }
    nodeArray->children.push_back(buildInputOrField("G", "G", PinType::Number, "180"));
    if (HasError) { delete nodeArray; return nullptr; }
    nodeArray->children.push_back(buildInputOrField("B", "B", PinType::Number, "255"));
    if (HasError) { delete nodeArray; return nullptr; }
    nodeArray->children.push_back(buildInputOrField("IsWall", "IsWall", PinType::Boolean, "false"));
    if (HasError) { delete nodeArray; return nullptr; }
    nodeArray->children.push_back(buildInputOrField("Distance", "Distance", PinType::Number, "0"));
    if (HasError) { delete nodeArray; return nullptr; }

    // If schema input is connected, prefer its neighbor list as default only when
    // explicit Neighbors input is not connected and no custom field is set.
    const Pin* neighborsPin = GetInputPinByName(n, "Neighbors");
    const bool hasNeighborsInput = (neighborsPin && resolver_.Resolve(neighborsPin->id));
    if (hasNeighborsInput)
        nodeArray->children.push_back(buildInputOrField("Neighbors", "Neighbors", PinType::Array, "[]"));
    else
        nodeArray->children.push_back(buildInputOrField("Neighbors", "Neighbors", PinType::Array, "[]"));
    if (HasError) { delete nodeArray; return nullptr; }

    return nodeArray;
}

Node* GraphCompiler::BuildGridNodeUpdate(const VisualNode& n)
{
    // Same slot layout as Create. Input Node pin is accepted for graph readability,
    // but update output is rebuilt from explicit fields/inputs for deterministic data.
    return BuildGridNodeCreate(n);
}

Node* GraphCompiler::BuildGridNodeDelete(const VisualNode& n)
{
    const Pin* nodesPin = GetInputPinByName(n, "Nodes");
    const Pin* indexPin = GetInputPinByName(n, "Index");
    if (!nodesPin || !indexPin)
    {
        Error("Grid Node Delete needs Nodes and Index inputs");
        return nullptr;
    }

    Node* nodesExpr = nullptr;
    if (const PinSource* nodesSrc = resolver_.Resolve(nodesPin->id))
        nodesExpr = BuildNode(*nodesSrc->node, nodesSrc->pinIdx);
    else
    {
        const std::string* nodesText = GetField(n, "Nodes");
        nodesExpr = BuildArrayLiteralNode(nodesText ? *nodesText : "[]");
    }

    Node* indexExpr = nullptr;
    if (const PinSource* indexSrc = resolver_.Resolve(indexPin->id))
        indexExpr = BuildNode(*indexSrc->node, indexSrc->pinIdx);
    else
    {
        const std::string* indexText = GetField(n, "Index");
        double indexValue = 0.0;
        if (indexText)
        {
            try { indexValue = std::stod(*indexText); }
            catch (...) { indexValue = 0.0; }
        }
        indexExpr = MakeNumberNode(indexValue);
    }

    if (HasError || !nodesExpr || !indexExpr)
    {
        delete nodesExpr;
        delete indexExpr;
        return nullptr;
    }

    Node* call = MakeNode(Token::FunctionCall);
    call->children.push_back(MakeIdNode("Array.RemoveIndex"));
    Node* params = MakeNode(Token::CallParams);
    params->children.push_back(nodesExpr);
    params->children.push_back(indexExpr);
    call->children.push_back(params);
    return call;
}

Node* GraphCompiler::BuildStructDefine(const VisualNode& n)
{
    const std::string* fields = GetField(n, "Fields");
    return BuildArrayLiteralNode(fields ? *fields : "[]");
}

Node* GraphCompiler::BuildStructCreate(const VisualNode& n)
{
    const std::string* schemaText = GetField(n, "Schema Fields");
    std::string inner = schemaText ? TrimCopy(*schemaText) : "";
    if (inner.size() >= 2 && inner.front() == '[' && inner.back() == ']')
        inner = inner.substr(1, inner.size() - 2);
    const std::vector<std::string> defs = SplitArrayItemsTopLevel(inner);

    auto parseBool = [](const std::string& s) -> bool
    {
        return s == "true" || s == "True" || s == "1";
    };

    auto defaultForType = [&](PinType t) -> Node*
    {
        switch (t)
        {
            case PinType::Number:  return MakeNumberNode(0.0);
            case PinType::Boolean: return MakeBoolNode(false);
            case PinType::String:  return MakeStringNode("");
            case PinType::Array:   return BuildArrayLiteralNode("[]");
            default:               return MakeNode(Token::Null);
        }
    };

    auto buildTypedLiteral = [&](PinType t, const std::string* raw) -> Node*
    {
        if (!raw)
            return defaultForType(t);

        switch (t)
        {
            case PinType::Number:
            {
                double v = 0.0;
                if (TryParseDoubleExact(*raw, v))
                    return MakeNumberNode(v);
                return MakeNumberNode(0.0);
            }
            case PinType::Boolean:
                return MakeBoolNode(parseBool(*raw));
            case PinType::String:
                return MakeStringNode(*raw);
            case PinType::Array:
                return BuildArrayLiteralNode(*raw);
            default:
                return MakeStringNode(*raw);
        }
    };

    Node* arr = MakeNode(Token::Array);
    for (std::string raw : defs)
    {
        raw = TrimCopy(raw);
        if (raw.size() >= 2 && ((raw.front() == '"' && raw.back() == '"') || (raw.front() == '\'' && raw.back() == '\'')))
            raw = raw.substr(1, raw.size() - 2);
        const size_t sep = raw.find(':');
        const std::string fieldName = (sep == std::string::npos) ? raw : TrimCopy(raw.substr(0, sep));
        const std::string typeName = (sep == std::string::npos) ? "Any" : TrimCopy(raw.substr(sep + 1));
        const PinType fieldType = VariableTypeFromString(typeName);

        if (const Pin* pin = GetInputPinByName(n, fieldName.c_str()))
        {
            if (const PinSource* src = resolver_.Resolve(pin->id))
            {
                arr->children.push_back(BuildNode(*src->node, src->pinIdx));
                if (HasError) { delete arr; return nullptr; }
                continue;
            }
        }

        const std::string* fieldValue = GetField(n, fieldName);
        arr->children.push_back(buildTypedLiteral(fieldType, fieldValue));
        if (HasError)
        {
            delete arr;
            return nullptr;
        }
    }
    return arr;
}

Node* GraphCompiler::BuildStructGetField(const VisualNode& n)
{
    const std::string* schemaText = GetField(n, "Schema Fields");
    const std::string* fieldName = GetField(n, "Field");
    if (!fieldName)
        return MakeNode(Token::Null);

    int index = -1;
    if (schemaText)
    {
        std::string inner = TrimCopy(*schemaText);
        if (inner.size() >= 2 && inner.front() == '[' && inner.back() == ']')
            inner = inner.substr(1, inner.size() - 2);
        const std::vector<std::string> defs = SplitArrayItemsTopLevel(inner);
        for (int i = 0; i < static_cast<int>(defs.size()); ++i)
        {
            std::string raw = TrimCopy(defs[static_cast<size_t>(i)]);
            if (raw.size() >= 2 && ((raw.front() == '"' && raw.back() == '"') || (raw.front() == '\'' && raw.back() == '\'')))
                raw = raw.substr(1, raw.size() - 2);
            const size_t sep = raw.find(':');
            const std::string name = (sep == std::string::npos) ? raw : TrimCopy(raw.substr(0, sep));
            if (name == *fieldName) { index = i; break; }
        }
    }

    const Pin* itemPin = GetInputPinByName(n, "Item");
    if (!itemPin || index < 0)
        return MakeNode(Token::Null);

    Node* itemExpr = BuildExpr(*itemPin);
    Node* idxExpr = MakeNumberNode(static_cast<double>(index));
    Node* indexer = MakeNode(Token::Indexer);
    indexer->children.push_back(itemExpr);
    indexer->children.push_back(idxExpr);
    return indexer;
}

Node* GraphCompiler::BuildStructSetField(const VisualNode& n)
{
    const std::string* schemaText = GetField(n, "Schema Fields");
    const std::string* fieldName = GetField(n, "Field");
    int index = -1;
    std::vector<std::string> defs;
    if (schemaText)
    {
        std::string inner = TrimCopy(*schemaText);
        if (inner.size() >= 2 && inner.front() == '[' && inner.back() == ']')
            inner = inner.substr(1, inner.size() - 2);
        defs = SplitArrayItemsTopLevel(inner);
        for (int i = 0; i < static_cast<int>(defs.size()); ++i)
        {
            std::string raw = TrimCopy(defs[static_cast<size_t>(i)]);
            if (raw.size() >= 2 && ((raw.front() == '"' && raw.back() == '"') || (raw.front() == '\'' && raw.back() == '\'')))
                raw = raw.substr(1, raw.size() - 2);
            const size_t sep = raw.find(':');
            const std::string name = (sep == std::string::npos) ? raw : TrimCopy(raw.substr(0, sep));
            if (fieldName && name == *fieldName) { index = i; break; }
        }
    }

    const Pin* itemPin = GetInputPinByName(n, "Item");
    const Pin* valuePin = GetInputPinByName(n, "Value");
    if (!itemPin || !valuePin)
        return MakeNode(Token::Null);

    // Rebuild whole array by reading each slot and replacing requested field.
    Node* arr = MakeNode(Token::Array);
    Node* itemExpr = BuildExpr(*itemPin);
    if (HasError || !itemExpr) { delete arr; delete itemExpr; return nullptr; }

    for (int i = 0; i < static_cast<int>(defs.size()); ++i)
    {
        if (i == index)
        {
            arr->children.push_back(BuildExpr(*valuePin));
            if (HasError) { delete arr; delete itemExpr; return nullptr; }
            continue;
        }
        Node* indexer = MakeNode(Token::Indexer);
        indexer->children.push_back(MakeIdNode("__struct_tmp"));
        indexer->children.push_back(MakeNumberNode(static_cast<double>(i)));
        arr->children.push_back(indexer);
    }

    // If schema missing, fallback to original item.
    if (defs.empty())
    {
        delete arr;
        return itemExpr;
    }

    // Wrap as Array.Copy-like temp-less approximation not available -> return rebuilt array using tmp variable pattern omitted.
    // Current fallback: if connected item is plain array literal/expr, returned array references __struct_tmp indexes,
    // so instead use original item expression only when schema unavailable. For schema path, prefer direct rebuild only if item is array literal.
    delete itemExpr;
    return arr;
}

Node* GraphCompiler::BuildStructDelete(const VisualNode& n)
{
    const Pin* arrayPin = GetInputPinByName(n, "Array");
    const Pin* idPin = GetInputPinByName(n, "Id");
    if (!idPin)
        idPin = GetInputPinByName(n, "Index"); // backward compatibility

    if (!arrayPin || !idPin)
    {
        Error("Struct Delete needs Array and Id inputs");
        return nullptr;
    }

    Node* arrayExpr = nullptr;
    if (const PinSource* arraySrc = resolver_.Resolve(arrayPin->id))
        arrayExpr = BuildNode(*arraySrc->node, arraySrc->pinIdx);
    else
    {
        const std::string* arrayText = GetField(n, "Array");
        arrayExpr = BuildArrayLiteralNode(arrayText ? *arrayText : "[]");
    }

    Node* idExpr = nullptr;
    if (const PinSource* idSrc = resolver_.Resolve(idPin->id))
        idExpr = BuildNode(*idSrc->node, idSrc->pinIdx);
    else
    {
        const std::string* idText = GetField(n, "Id");
        if (!idText)
            idText = GetField(n, "Index"); // backward compatibility

        double idValue = 0.0;
        if (idText)
        {
            try { idValue = std::stod(*idText); }
            catch (...) { idValue = 0.0; }
        }
        idExpr = MakeNumberNode(idValue);
    }

    if (HasError || !arrayExpr || !idExpr)
    {
        delete arrayExpr;
        delete idExpr;
        return nullptr;
    }

    Node* call = MakeNode(Token::FunctionCall);
    call->children.push_back(MakeIdNode("Array.RemoveIndex"));
    Node* params = MakeNode(Token::CallParams);
    params->children.push_back(arrayExpr);
    params->children.push_back(idExpr);
    call->children.push_back(params);
    return call;
}

Node* GraphCompiler::BuildVariable(const VisualNode& n)
{
    const std::string* nameStr = GetField(n, "Name");
    const std::string* typeStr = GetField(n, "Type");
    const std::string* defaultStr = GetField(n, "Default");
    const std::string* variantStr = GetField(n, "Variant");

    std::string varName = nameStr ? *nameStr : "__unnamed";

    // Compile behavior is driven by Variant instead of inferred pin layout.
    const bool isGetVariant = (variantStr && *variantStr == "Get");
    if (isGetVariant)
        return MakeIdNode(varName);

    const PinType defaultType = VariableTypeFromString(typeStr ? *typeStr : "Number");
    const std::string defaultValue = defaultStr ? *defaultStr : "0.0";

    const Pin* setInput = GetInputPinByName(n, "Default");
    if (!setInput)
        setInput = GetInputPinByName(n, "Set"); // backward compatibility

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
            rhs = MakeStringNode(defaultValue);
            break;
        case PinType::Array:
            rhs = BuildArrayLiteralNode(defaultValue);
            break;
        case PinType::Number:
            try { rhs = MakeNumberNode(std::stod(defaultValue)); }
            catch (...) { rhs = MakeNumberNode(0.0); }
            break;
        default:
            rhs = MakeNode(Token::Null);
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
        Node* valueExpr = nullptr;

        const PinSource* valueSrc = resolver_.Resolve(valuePin->id);
        if (valueSrc)
        {
            valueExpr = BuildNode(*valueSrc->node, valueSrc->pinIdx);
            if (HasError) { delete scope; return nullptr; }
        }
        else
        {
            // QoL fallback:
            // If Debug Print value is unconnected but this node is executed from
            // a ForEach Body flow, default to that ForEach's current Element value.
            const Pin* flowIn = GetInputPinByName(n, "In");
            if (!flowIn)
                flowIn = GetInputPinByName(n, "Flow");

            if (flowIn)
            {
                const FlowTarget* flowSrc = resolver_.ResolveFlow(flowIn->id);
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
                        valueExpr = BuildNode(*flowSrc->node, elementOutIdx);
                    if (HasError) { delete scope; return nullptr; }
                }
            }

            if (!valueExpr)
                valueExpr = BuildExpr(*valuePin);
            if (HasError) { delete scope; return nullptr; }
        }

        Node* printValue = MakeNode(Token::FunctionCall);
        printValue->children.push_back(MakeIdNode("println"));
        Node* valueParams = MakeNode(Token::CallParams);
        valueParams->children.push_back(valueExpr);
        printValue->children.push_back(valueParams);
        scope->children.push_back(printValue);
    }

    return scope;
}

Node* GraphCompiler::BuildCallFunction(const VisualNode& n)
{
    const std::string* nameStr = GetField(n, "Name");
    const std::string funcName = (nameStr && !nameStr->empty())
        ? *nameStr : "__unnamed_func";
    const std::string tempVar = "__call_result_" +
        std::to_string(static_cast<uintptr_t>(n.id.Get()));

    Node* call = MakeNode(Token::FunctionCall);
    call->children.push_back(MakeIdNode(funcName));

    Node* params = MakeNode(Token::CallParams);
    for (const Pin& pin : n.inPins)
    {
        if (pin.type == PinType::Flow) continue;
        Node* arg = BuildExpr(pin);
        if (HasError) { delete call; return nullptr; }
        params->children.push_back(arg);
    }
    call->children.push_back(params);

    Node* assign = MakeNode(Token::Assign);
    assign->children.push_back(MakeIdNode(tempVar));
    assign->children.push_back(call);
    return assign;
}

Node* GraphCompiler::BuildFunction(const VisualNode& n)
{
    const std::string* nameStr = GetField(n, "Name");
    const std::string funcName = (nameStr && !nameStr->empty())
        ? *nameStr : "__unnamed_func";

    Node* funcDecl = MakeNode(Token::FunctionDef);
    funcDecl->data = funcName;

    Node* params = MakeNode(Token::CallParams);
    for (int i = 0; ; ++i)
    {
        const std::string* p = GetField(n, "Param" + std::to_string(i));
        if (!p || p->empty()) break;

        Node* param = MakeNode(Token::Id);
        param->data = *p;
        Node* typeNode = MakeNode(Token::AnyType);
        param->children.push_back(typeNode);
        params->children.push_back(param);
    }
    funcDecl->children.push_back(params);

    Node* body = MakeNode(Token::Scope);

    // Deklaroi loop last index muuttujat funktion bodyyn
    if (nodes_)
    {
        for (const VisualNode& loopNode : *nodes_)
        {
            if (!loopNode.alive || loopNode.nodeType != NodeType::Loop)
                continue;

            double startVal = 0.0;
            const Pin* startPin = GetInputPinByName(loopNode, "Start");
            const bool startConnected = (startPin && resolver_.Resolve(startPin->id));
            if (!startConnected)
            {
                const std::string* startStr = GetField(loopNode, "Start");
                if (startStr)
                {
                    try { startVal = std::stod(*startStr); }
                    catch (...) { startVal = 0.0; }
                }
            }

            Node* loopLastDecl = MakeNode(Token::VarDeclare);
            loopLastDecl->data = LoopLastIndexVarName(loopNode);
            loopLastDecl->children.push_back(MakeNode(Token::TypeNumber));
            loopLastDecl->children.push_back(MakeNumberNode(std::round(startVal)));
            body->children.push_back(loopLastDecl);
        }
    }

    if (const Pin* bodyOut = GetOutputPinByName(n, "Body"))
        AppendFlowChainFromOutput(bodyOut->id, body);
    if (HasError) { delete funcDecl; return nullptr; }

    if (!body->children.empty())
    {
        Node* lastStmt = body->children.back();
        body->children.pop_back();

        Node* ret = MakeNode(Token::Return);
        ret->children.push_back(lastStmt);
        body->children.push_back(ret);
    }

    funcDecl->children.push_back(body);
    return funcDecl;
}

