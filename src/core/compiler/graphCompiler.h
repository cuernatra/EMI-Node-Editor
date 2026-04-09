/** @file graphCompiler.h */
/**
 * Turns a visual graph (boxes + wires) into an AST the VM can run.
 *
 * Flow wires decide what runs next. Data wires build values.
 * Node logic lives in NodeDescriptor callbacks — this class provides
 * the tools those callbacks use: resolving links, building expressions,
 * and walking the execution chain.
 */

#pragma once
#include "../graphModel/visualNode.h"
#include "../graphModel/link.h"
#include "pinResolver.h"
#include "Parser/Node.h"
#include "Parser/Lexer.h"
#include <string>
#include <vector>
#include <unordered_set>

/**
 * Builds a script AST from a connected visual graph.
 *
 * Returns a root Scope containing a generated __graph__() function
 * plus any top-level definitions nodes produce (e.g. user functions).
 */
class GraphCompiler
{
public:
    bool HasError = false;

    /** Name of the generated graph entry-point function. */
    static constexpr const char* kGraphFunctionName = "__graph__";

    /**
     * Compile a graph snapshot into an AST tree.
     * Returns the root Scope on success, nullptr on failure (HasError is set).
     * Caller owns the returned node and must delete it.
     */
    Node* Compile(const std::vector<VisualNode>& nodes,
                  const std::vector<Link>&       links);

    /** Last compile error text. */
    const std::string& GetError() const { return errorMsg_; }

    /** Record a compile error and stop compilation. */
    void Error(const std::string& msg);

    /** Find which output pin feeds a given input pin. Returns nullptr if disconnected. */
    const PinSource* Resolve(const Pin& inputPin) const { return resolver_.Resolve(inputPin.id); }

    /** Find where a flow output wire leads next. Returns nullptr if nothing is connected. */
    const FlowTarget* ResolveFlow(const Pin& outputPin) const { return resolver_.ResolveFlow(outputPin.id); }

    /**
     * Build the expression that feeds an input pin.
     *
     * If wired, compiles the upstream node. If unwired, returns a safe default
     * (0 / false / "" / []). Also returns the default if the upstream node has
     * flow pins but hasn't been reached by the execution chain yet.
     */
    Node* BuildExpr(const Pin& inputPin);

    /**
     * Compile a visual node into an AST node.
     *
     * outPinIdx selects which output pin is being requested — matters when a node
     * has multiple data outputs with different values (e.g. Loop → Index vs LastIndex).
     */
    Node* BuildNode(const VisualNode& n, int outPinIdx = 0);

    /** Parse an array literal string like "[1, "hi", [2,3]]" into an AST Array node. */
    Node* BuildArrayLiteralNode(const std::string& text) const;

    /**
     * Walk the flow chain starting at a flow output pin and append each
     * compiled statement into targetScope.
     * Flow-control nodes (if/loop/etc.) handle their own sub-scopes via compileFlow.
     */
    void AppendFlowChainFromOutput(ed::PinId flowOutputPinId, Node* targetScope);

    /** Call before/after compiling a loop body. nodeIdKey is the loop node's id cast to uintptr_t. */
    void PushActiveLoopBodyNodeId(uintptr_t nodeIdKey);
    void PopActiveLoopBodyNodeId(uintptr_t nodeIdKey);

    /** Call before/after compiling a ForEach body. */
    void PushActiveForEachBodyNodeId(uintptr_t nodeIdKey);
    void PopActiveForEachBodyNodeId(uintptr_t nodeIdKey);

    /** True if currently compiling inside the body of this loop node. */
    bool IsActiveLoopBodyNodeId(uintptr_t nodeIdKey) const;
    /** True if currently compiling inside the body of this ForEach node. */
    bool IsActiveForEachBodyNodeId(uintptr_t nodeIdKey) const;

    /**
     * Reset flow-walk state so an independent flow chain can be compiled from scratch.
     * Used when compiling user function bodies.
     */
    void ResetFlowTraversalState();

    /**
     * Declare a graph variable name once. Returns false if already declared.
     * Used by compilePrelude callbacks to avoid duplicate var declarations.
     */
    bool TryDeclareGraphVariable(const std::string& varName);

    /** Find a user-defined Function node by its Name field. Returns nullptr if not found. */
    const VisualNode* FindUserFunctionByName(const std::string& name) const;

private:
    PinResolver resolver_;

    void CollectFlowReachableFromOutput(ed::PinId flowOutputPinId);
    void AppendFlowNode(const VisualNode& n, int triggeredInputPinIdx, Node* targetScope);
    bool NodeRequiresFlow(const VisualNode& n) const;
    const VisualNode* FindFirstNode(NodeType type) const;

    std::string errorMsg_;

    std::unordered_set<uintptr_t> activeNodeBuilds_;
    std::unordered_set<uintptr_t> activeLoopBodyNodeIds_;
    std::unordered_set<uintptr_t> activeForEachBodyNodeIds_;
    std::unordered_set<uintptr_t> flowReachableNodes_;
    std::unordered_set<uintptr_t> activeFlowOutputs_;
    std::unordered_set<uintptr_t> visitedFlowOutputs_;

    std::unordered_set<std::string> declaredGraphVariables_;

    // Counts AppendFlowChainFromOutput calls — used in debug to detect
    // compileFlow callbacks that silently drop connected flow outputs.
    size_t appendFlowInvocations_ = 0;

    const std::vector<VisualNode>* nodes_ = nullptr;
};
