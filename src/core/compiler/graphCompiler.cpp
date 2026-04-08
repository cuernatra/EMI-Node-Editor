// Pipeline: index links → mark reachable nodes → prelude → flow chain → top-level defs.
// See Compile() for details. BuildExpr returns safe defaults when a pin is disconnected
// or its source node hasn't run yet (bypassFlowReachableCheck skips the latter guard).

#include "graphCompiler.h"
#include "../registry/nodeRegistry.h"
#include "astBuilders.h"
#include "textParseHelpers.h"
#include "../graph/visualNodeUtils.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>

using emi::ast::MakeBoolLiteral;
using emi::ast::MakeIdNode;
using emi::ast::MakeNode;
using emi::ast::MakeNumberLiteral;
using emi::ast::MakeStringLiteral;

void GraphCompiler::PushActiveLoopBodyNodeId(uintptr_t nodeIdKey)
{
    activeLoopBodyNodeIds_.insert(nodeIdKey);
}

void GraphCompiler::PopActiveLoopBodyNodeId(uintptr_t nodeIdKey)
{
    activeLoopBodyNodeIds_.erase(nodeIdKey);
}

void GraphCompiler::PushActiveForEachBodyNodeId(uintptr_t nodeIdKey)
{
    activeForEachBodyNodeIds_.insert(nodeIdKey);
}

void GraphCompiler::PopActiveForEachBodyNodeId(uintptr_t nodeIdKey)
{
    activeForEachBodyNodeIds_.erase(nodeIdKey);
}

bool GraphCompiler::IsActiveLoopBodyNodeId(uintptr_t nodeIdKey) const
{
    return activeLoopBodyNodeIds_.find(nodeIdKey) != activeLoopBodyNodeIds_.end();
}

bool GraphCompiler::IsActiveForEachBodyNodeId(uintptr_t nodeIdKey) const
{
    return activeForEachBodyNodeIds_.find(nodeIdKey) != activeForEachBodyNodeIds_.end();
}

void GraphCompiler::ResetFlowTraversalState()
{
    visitedFlowOutputs_.clear();
    activeFlowOutputs_.clear();
}

bool GraphCompiler::TryDeclareGraphVariable(const std::string& varName)
{
    return declaredGraphVariables_.insert(varName).second;
}

const VisualNode* GraphCompiler::FindUserFunctionByName(const std::string& name) const
{
    if (!nodes_)
        return nullptr;

    for (const VisualNode& candidate : *nodes_)
    {
        if (!candidate.alive || candidate.nodeType != NodeType::Function)
            continue;
        const std::string* fnName = FindField(candidate, "Name");
        if (fnName && *fnName == name)
            return &candidate;
    }
    return nullptr;
}

void GraphCompiler::Error(const std::string& msg)
{
    errorMsg_ = msg;
    HasError = true;
}

// True if the node has both a flow input and a flow output — used by BuildExpr
// to decide whether to guard data reads behind the flow-reachability check.
bool GraphCompiler::NodeRequiresFlow(const VisualNode& n) const
{
    bool hasFlowIn  = false;
    bool hasFlowOut = false;

    for (const Pin& p : n.inPins)
    {
        if (p.type == PinType::Flow) { hasFlowIn = true; break; }
    }

    for (const Pin& p : n.outPins)
    {
        if (p.type == PinType::Flow) { hasFlowOut = true; break; }
    }

    return hasFlowIn && hasFlowOut;
}

const VisualNode* GraphCompiler::FindFirstNode(NodeType type) const
{
    if (!nodes_) return nullptr;

    for (const VisualNode& n : *nodes_)
    {
        if (n.alive && n.nodeType == type)
            return &n;
    }

    return nullptr;
}

// Recursively marks every node reachable via execution edges from a flow output.
// BuildExpr checks flowReachableNodes_ to reject data from nodes that haven't run.
void GraphCompiler::CollectFlowReachableFromOutput(ed::PinId flowOutputPinId)
{
    const uintptr_t outKey = static_cast<uintptr_t>(flowOutputPinId.Get());

    // Cycle guard – stop if we are already walking this output.
    if (!activeFlowOutputs_.insert(outKey).second)
        return;

    struct ScopedErase
    {
        std::unordered_set<uintptr_t>& set;
        uintptr_t key;
        ~ScopedErase() { set.erase(key); }
    } guard{ activeFlowOutputs_, outKey };

    // Visit guard – stop if we have already recorded this output.
    if (!visitedFlowOutputs_.insert(outKey).second)
        return;

    const FlowTarget* target = resolver_.ResolveFlow(flowOutputPinId);
    if (!target || !target->node)
        return;

    const VisualNode& n = *target->node;
    flowReachableNodes_.insert(static_cast<uintptr_t>(n.id.Get()));

    // Recurse into every flow output of the reached node.
    for (const Pin& outPin : n.outPins)
    {
        if (outPin.type == PinType::Flow)
            CollectFlowReachableFromOutput(outPin.id);
    }
}

void GraphCompiler::AppendFlowChainFromOutput(ed::PinId flowOutputPinId, Node* targetScope)
{
    ++appendFlowInvocations_;

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

    // compileFlow nodes (Branch, Loop, Sequence, …) own their sub-scopes and
    // must call AppendFlowChainFromOutput for every downstream flow output.
    if (const NodeDescriptor* desc = NodeRegistry::Get().Find(n.nodeType))
    {
        if (desc->compileFlow)
        {
            const size_t invocationsBefore = appendFlowInvocations_;
            desc->compileFlow(this, n, targetScope);

#ifdef DEBUG
            // Detect compileFlow callbacks that silently drop connected flow outputs.
            // If this node has at least one connected flow output but AppendFlowChainFromOutput
            // was never called, downstream execution is lost with no error reported.
            if (!HasError && appendFlowInvocations_ == invocationsBefore)
            {
                for (const Pin& p : n.outPins)
                {
                    if (p.type == PinType::Flow && resolver_.ResolveFlow(p.id))
                    {
                        assert(false &&
                            "compileFlow callback has connected flow output(s) but never "
                            "called AppendFlowChainFromOutput — downstream execution is silently dropped. "
                            "See compiler/README.md 'How to add a flow-control node'.");
                        break;
                    }
                }
            }
#endif
            return;
        }
    }

    // Default: compile to a statement, push it, then follow the "Out" flow pin.
    Node* stmt = BuildNode(n);
    if (HasError)
    {
        delete stmt;
        return;
    }

    if (stmt)
        targetScope->children.push_back(stmt);

    const Pin* outFlow = FindOutputPin(n, "Out");
    if (!outFlow)
    {
        for (const Pin& p : n.outPins)
        {
            if (p.type == PinType::Flow) { outFlow = &p; break; }
        }
    }

    if (outFlow)
        AppendFlowChainFromOutput(outFlow->id, targetScope);
}

// Output AST shape:
//
//   Scope (root)
//   ├─ VarDeclare __call_result_N    ← CallFunction prelude
//   ├─ FunctionDef myFunc(…)         ← user Function top-level
//   │   └─ Scope (fn body)
//   └─ FunctionDef __graph__()
//       └─ Scope (body)
//           ├─ VarDeclare myVar           ← Variable prelude
//           ├─ VarDeclare __loop_last_i_N ← Loop prelude
//           └─ … flow chain statements
Node* GraphCompiler::Compile(const std::vector<VisualNode>& nodes,
                             const std::vector<Link>& links)
{
    // ── Reset state ───────────────────────────────────────────────────────────
    HasError = false;
    errorMsg_.clear();
    nodes_ = &nodes;
    declaredGraphVariables_.clear();

    activeNodeBuilds_.clear();
    activeLoopBodyNodeIds_.clear();
    activeForEachBodyNodeIds_.clear();
    flowReachableNodes_.clear();
    activeFlowOutputs_.clear();
    visitedFlowOutputs_.clear();

    // ── Stage 1: Link indexing ────────────────────────────────────────────────
    // Build fast lookup maps from the link list so all later stages can resolve
    // "what feeds this input?" and "where does this flow output go?" in O(1).
    resolver_.Build(nodes, links);

    const VisualNode* startNode = FindFirstNode(NodeType::Start);
    if (!startNode)
    {
        Error("Graph requires a Start node to drive flow execution");
        return nullptr;
    }

    const Pin* startOut = FindOutputPin(*startNode, "Exec");
    if (!startOut)
    {
        for (const Pin& p : startNode->outPins)
        {
            if (p.type == PinType::Flow) { startOut = &p; break; }
        }
    }

    if (!startOut)
    {
        Error("Start node has no flow output pin");
        return nullptr;
    }

    // ── Stage 2: Flow reachability ────────────────────────────────────────────
    // Mark every node that can be reached via execution edges. BuildExpr checks
    // this set before following a data link from a flow-gated node — if the
    // source node hasn't run, its output should be treated as a default value.

    // Main graph execution chain starting from Start.
    CollectFlowReachableFromOutput(startOut->id);

    // Also mark nodes inside user Function bodies. Detected via compileTopLevel
    // being non-null (no hard-coded NodeType::Function check needed).
    for (const VisualNode& n : nodes)
    {
        if (!n.alive) continue;
        const NodeDescriptor* desc = NodeRegistry::Get().Find(n.nodeType);
        if (!desc || !desc->compileTopLevel) continue;  // ← compileTopLevel used here

        const Pin* fnOut = FindOutputPin(n, "Out");
        if (!fnOut)
        {
            for (const Pin& p : n.outPins)
            {
                if (p.type == PinType::Flow) { fnOut = &p; break; }
            }
        }
        if (fnOut) CollectFlowReachableFromOutput(fnOut->id);
    }

    // ── Stage 3: Prelude ──────────────────────────────────────────────────────
    // Ask every node whose descriptor has compilePrelude to emit its required
    // declarations. This runs before the flow chain so that all variables and
    // temp results are declared at the correct scope before they are used.
    //
    // NodeDescriptor::compilePrelude  signature:
    //   void(GraphCompiler*, const VisualNode&, Node* rootScope, Node* bodyScope)
    //
    // Callbacks write into whichever scope they need:
    //   • rootScope (global) – CallFunction result temps (__call_result_N)
    //   • bodyScope          – Variable VarDeclares, Loop __loop_last_i_N temps

    Node* root = MakeNode(Token::Scope);   // global scope
    Node* body = MakeNode(Token::Scope);   // __graph__() body scope

    for (const VisualNode& n : nodes)
    {
        if (!n.alive) continue;
        const NodeDescriptor* desc = NodeRegistry::Get().Find(n.nodeType);
        if (!desc || !desc->compilePrelude) continue;  // ← compilePrelude used here

        desc->compilePrelude(this, n, root, body);
        if (HasError) { delete root; delete body; return nullptr; }
    }

    // ── Stage 4: Flow chain ───────────────────────────────────────────────────
    // Walk execution links from Start and build the body of __graph__().
    // Each node in the chain is dispatched through AppendFlowNode (see above),
    // which uses NodeDescriptor::compileFlow for flow-control nodes and falls
    // back to BuildNode + "Out" pin following for everything else.

    visitedFlowOutputs_.clear();   // reset so the flow walk can revisit anything
    activeFlowOutputs_.clear();    // the reachability walk may have set these

    AppendFlowChainFromOutput(startOut->id, body);  // ← compileFlow / compile used here
    if (HasError) { delete root; delete body; return nullptr; }

    // Wrap the compiled body in the graph entrypoint function definition.
    Node* funcDecl = MakeNode(Token::FunctionDef);
    funcDecl->data = std::string(kGraphFunctionName);
    funcDecl->children.push_back(MakeNode(Token::CallParams));  // no parameters
    funcDecl->children.push_back(body);

    // ── Stage 5: Top-level definitions ───────────────────────────────────────
    // Ask every node with compileTopLevel to emit a top-level AST child.
    // Currently only user Function nodes use this: each produces a FunctionDef
    // that sits at the root scope alongside __graph__().
    //
    // NodeDescriptor::compileTopLevel  signature:
    //   Node*(GraphCompiler*, const VisualNode&)
    // Return the AST node to insert, or nullptr to skip.

    for (const VisualNode& n : nodes)
    {
        if (!n.alive) continue;
        const NodeDescriptor* desc = NodeRegistry::Get().Find(n.nodeType);
        if (!desc || !desc->compileTopLevel) continue;  // ← compileTopLevel used here

        Node* topLevel = desc->compileTopLevel(this, n);
        if (HasError) { delete topLevel; delete root; delete funcDecl; return nullptr; }
        if (topLevel)
            root->children.push_back(topLevel);
    }

    root->children.push_back(funcDecl);
    return root;
}

Node* GraphCompiler::BuildExpr(const Pin& inputPin)
{
    auto makeDefaultValue = [&]() -> Node*
    {
        switch (inputPin.type)
        {
            case PinType::Number:  return MakeNumberLiteral(0.0);
            case PinType::Boolean: return MakeBoolLiteral(false);
            case PinType::String:  return MakeStringLiteral("");
            case PinType::Array:   return MakeNode(Token::Array);
            default:               return MakeNode(Token::Null);
        }
    };

    const PinSource* src = resolver_.Resolve(inputPin.id);
    if (!src)
        return makeDefaultValue();  // pin is not connected → use default

    // ── NodeDescriptor::bypassFlowReachableCheck ──────────────────────────────
    bool bypassFlowCheck = false;
    if (src->node)
    {
        const NodeDescriptor* srcDesc = NodeRegistry::Get().Find(src->node->nodeType);
        bypassFlowCheck = srcDesc && srcDesc->bypassFlowReachableCheck;
    }

    if (!bypassFlowCheck && src->node && NodeRequiresFlow(*src->node))
    {
        const uintptr_t key = static_cast<uintptr_t>(src->node->id.Get());
        if (flowReachableNodes_.find(key) == flowReachableNodes_.end())
            return makeDefaultValue();  // source node hasn't run → safe default
    }

    return BuildNode(*src->node, src->pinIdx);
}

Node* GraphCompiler::BuildNode(const VisualNode& n, int outPinIdx)
{
    // Cycle guard: detect graphs where a node's output feeds back into its own input.
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

    const NodeDescriptor* desc = NodeRegistry::Get().Find(n.nodeType);

    // ── Step 1: NodeDescriptor::compileOutput ─────────────────────────────────
    if (desc && desc->compileOutput)
    {
        Node* out = desc->compileOutput(this, n, outPinIdx);
        if (HasError) { delete out; return nullptr; }
        if (out) return out;  // non-null → done; nullptr → fall through
    }

    // ── Step 2: NodeDescriptor::compile ──────────────────────────────────────
    if (desc && desc->compile)
        return desc->compile(this, n);

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
            arr->children.push_back(MakeStringLiteral(item.substr(1, item.size() - 2)));
            continue;
        }

        if (item == "true" || item == "True")  { arr->children.push_back(MakeBoolLiteral(true));         continue; }
        if (item == "false" || item == "False") { arr->children.push_back(MakeBoolLiteral(false));        continue; }
        if (item == "null"  || item == "Null")  { arr->children.push_back(MakeNode(Token::Null));      continue; }

        if (item.front() == '[' && item.back() == ']')
        {
            arr->children.push_back(BuildArrayLiteralNode(item));
            continue;
        }

        double numeric = 0.0;
        if (TryParseDoubleExact(item, numeric))
        {
            arr->children.push_back(MakeNumberLiteral(numeric));
            continue;
        }

        arr->children.push_back(MakeStringLiteral(item));
    }

    return arr;
}
