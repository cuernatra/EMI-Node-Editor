/**
 * @file graphCompiler.cpp
 * @brief Graph → EMI-Script AST compiler implementation.
 *
 * Pipeline (GraphCompiler::Compile):
 * 1) Index links (PinResolver)
 * 2) Mark flow-reachable nodes (guards data reads)
 * 3) Prelude (NodeDescriptor::compilePrelude)
 * 4) Flow chain (NodeDescriptor::compileFlow or BuildNode + "Out")
 * 5) Top-level defs (NodeDescriptor::compileTopLevel)
 *
 * Dispatch rules:
 * - BuildNode: compileOutput (optional) → compile
 * - BuildExpr: returns safe defaults when disconnected or flow-not-reachable,
 *   unless bypassFlowReachableCheck is enabled.
 */

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

/// Loop/ForEach body guards used by compileFlow callbacks.

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

/// Helpers used by compileTopLevel callbacks (e.g. user-defined functions).

/// Reset flow-walk caches before compiling an independent flow chain.
void GraphCompiler::ResetFlowTraversalState()
{
    visitedFlowOutputs_.clear();
    activeFlowOutputs_.clear();
}

/// Declare a graph variable once (used by prelude callbacks).
bool GraphCompiler::TryDeclareGraphVariable(const std::string& varName)
{
    return declaredGraphVariables_.insert(varName).second;
}

/// Find a user Function node by its Name field.
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

/// Small AST builders used by node compile callbacks.

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

// ─────────────────────────────────────────────────────────────────────────────
// Internal graph-query helpers
// ─────────────────────────────────────────────────────────────────────────────

// Returns true if the node has at least one flow input AND one flow output.
// Used by BuildExpr to decide whether a source node is "flow-gated"
// (its output should only be used if it has been executed this frame).
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

// ─────────────────────────────────────────────────────────────────────────────
// Stage 2 helper – flow-reachability walk
//
// Recursively marks every node reachable via execution edges from a given flow
// output pin. The result (flowReachableNodes_) is consumed by BuildExpr to
// reject data reads from nodes that cannot have run yet.
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
// Stage 4 helpers – execution-chain compilation
//
// AppendFlowChainFromOutput  follows one flow-output link and delegates to
//   AppendFlowNode for the node at the other end.
//
// AppendFlowNode  compiles one node in the execution chain:
//   • If NodeDescriptor::compileFlow is set → the callback owns everything:
//     building the node AST, populating sub-scopes (e.g. if/else, loop body),
//     and following any downstream flow outputs.
//   • Otherwise → default path: call BuildNode() to get a statement AST node,
//     push it into targetScope, then follow the "Out" flow pin.
// ─────────────────────────────────────────────────────────────────────────────

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

    // ── NodeDescriptor::compileFlow ───────────────────────────────────────────
    // Flow-control nodes (Branch, Loop, While, Sequence, …) register this
    // callback. It is responsible for:
    //   • Building the control-flow AST node (if/for/while/…)
    //   • Populating any sub-scopes (true/false branches, loop body, …)
    //   • Calling AppendFlowChainFromOutput for each downstream flow output
    // If compileFlow is set, we hand off entirely — no default chaining.
    if (const NodeDescriptor* desc = NodeRegistry::Get().Find(n.nodeType))
    {
        if (desc->compileFlow)
        {
            desc->compileFlow(this, n, targetScope);
            return;
        }
    }

    // ── Default flow path ─────────────────────────────────────────────────────
    // For nodes without compileFlow (most statement nodes: set-variable, print,
    // draw-cell, call-function, …): compile to a single statement via BuildNode,
    // push it, then follow the "Out" flow pin to continue the chain.
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

// ─────────────────────────────────────────────────────────────────────────────
// Compile()  –  main entry point
//
// Runs the five compilation stages described in the file-header comment and
// returns the root AST Scope node, or nullptr on error (HasError will be set).
//
// Output AST structure:
//
//   Scope (root)
//   ├─ VarDeclare __call_result_N   ← from CallFunction compilePrelude
//   ├─ FunctionDef myFunc(…)        ← from user Function compileTopLevel
//   │   └─ Scope (fn body)
//   └─ FunctionDef __graph__()
//       └─ Scope (body)
//           ├─ VarDeclare myVar     ← from Variable compilePrelude
//           ├─ VarDeclare __loop_last_i_N  ← from Loop compilePrelude
//           └─ … statements from flow chain
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
// BuildExpr  –  resolve one input pin to an expression AST node
//
// Called from compile callbacks to build
// the expression tree feeding into a specific input pin.
//
// Flow-reachability guard:
//   If a data link comes from a node that also has flow pins (NodeRequiresFlow),
//   we check that the source node is in flowReachableNodes_. If it isn't, the
//   node cannot have executed yet, so we return a safe default value instead.
//
//   NodeDescriptor::bypassFlowReachableCheck = true skips this guard. Used by
//   CallFunction whose Result pin carries data but whose execution depends on
//   where the node appears in the flow chain, not on a separate reachability test.
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
// BuildNode  –  compile one node into an expression or statement AST node
//
// outPinIdx selects which output pin is being requested (matters for nodes
// whose different outputs carry different values, e.g. Loop Index vs LastIndex).
//
// Dispatch order (first match wins):
//
//   1. NodeDescriptor::compileOutput  – per-pin value override
//      If set, called first. Return a non-null Node* to short-circuit.
//      Return nullptr to fall through to the next step.
//      Example: Loop returns __loop_i_N or __loop_last_i_N based on outPinIdx.
//
//   2. NodeDescriptor::compile        – explicit callback
//      Full control: receives (GraphCompiler*, const VisualNode&), returns Node*.
//      Example: OperatorNode reads "Op" field, resolves A and B pins, calls
//               EmitBinaryOp(token, lhs, rhs).
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
// BuildArrayLiteralNode  –  parse "[a, b, c]" text into an Array AST node
//
// Used by compilePrelude / compile callbacks when a pin carries an array
// literal stored as a field string (e.g. Variable default value, ArrayGet
// fallback). Handles nested arrays, quoted strings, booleans, and numbers.
// ─────────────────────────────────────────────────────────────────────────────

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
