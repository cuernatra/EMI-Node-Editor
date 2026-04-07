/** @file graphCompiler.h */
/**
 * @brief Turns a visual graph (nodes + wires) into an executable EMI-Script AST.
 *
 * If you are adding or editing nodes in `src/core/registry/nodes/*.cpp`, you do
 * not need to know traditional compiler theory.
 *
 * Practical model:
 * - The visual graph is like a program made out of boxes.
 * - Flow pins decide statement order ("what runs next").
 * - Data pins build expressions ("what value is used").
 * - This compiler converts that into an AST tree the VM can run.
 *
 * Node-specific logic lives in `NodeDescriptor` callbacks. This class provides
 * the shared services those callbacks use: following links, building
 * expressions/statements, emitting common AST patterns, and walking flow chains.
 */

#pragma once
#include "../graph/visualNode.h"
#include "../graph/link.h"
#include "pinResolver.h"
#include "Parser/Node.h"
#include "Parser/Lexer.h"
#include <string>
#include <vector>
#include <unordered_set>

/**
 * @brief Builds script AST from connected visual nodes.
 *
 * The returned AST is a root `Scope` that contains a generated `__graph__()`
 * function (and any other top-level definitions produced by nodes).
 */
class GraphCompiler
{
public:
    /// Set to true if compilation fails. Most helpers early-out if this is set.
    bool HasError = false;

    /** Name of the generated function used as compiled graph entrypoint. */
    static constexpr const char* kGraphFunctionName = "__graph__";

    /**
     * @brief Compile graph data (nodes + links) into an AST tree.
     *
     * @param nodes All visual nodes in the snapshot.
     * @param links All links (wires) between pins.
     * @return Root AST Scope node on success, or nullptr on failure.
     *
     * Ownership:
     * - Caller owns the returned node and must delete it.
     */
    Node* Compile(const std::vector<VisualNode>& nodes,
                  const std::vector<Link>&       links);

    /**
     * @brief Get last compile error text.
     * @return Error message set by Error(), or empty if no error.
     */
    const std::string& GetError() const { return errorMsg_; }

    /**
     * @brief Record a compilation error message and mark the compiler as failed.
     *
     * Most node compile callbacks call this to report a user-facing error.
     *
     * @param msg Error text.
     */
    void Error(const std::string& msg);

    /**
     * @brief Find which output pin feeds an input pin.
     *
     * @param inputPin Input pin on the current node.
     * @return PinSource with upstream node + output index, or nullptr if unconnected.
     */
    const PinSource* Resolve(const Pin& inputPin) const { return resolver_.Resolve(inputPin.id); }

    /**
     * @brief Find where a flow output continues next.
     *
     * @param outputPin Flow output pin.
     * @return FlowTarget with downstream node + input index, or nullptr if not connected.
     */
    const FlowTarget* ResolveFlow(const Pin& outputPin) const { return resolver_.ResolveFlow(outputPin.id); }


    // Small AST building helpers used by node compile callbacks.
    /**
     * @brief Emit a binary operator AST node.
     *
     * @param op Operator token (e.g. Token::Add).
     * @param lhs Left expression (owned by the returned node on success).
     * @param rhs Right expression (owned by the returned node on success).
     * @return Newly allocated operator node, or nullptr if lhs/rhs are null.
     */
    Node* EmitBinaryOp(Token op, Node* lhs, Node* rhs) const;

    /**
     * @brief Emit a unary operator AST node.
     *
     * @param op Operator token (e.g. Token::Not).
     * @param operand Operand expression (owned by the returned node on success).
     * @return Newly allocated operator node, or nullptr if operand is null.
     */
    Node* EmitUnaryOp(Token op, Node* operand) const;

    /**
     * @brief Emit a function call AST node.
     *
     * @param name Function name.
     * @param args Argument nodes. Non-null args become children of the call.
     * @return Newly allocated function call node.
     */
    Node* EmitFunctionCall(const std::string& name, std::vector<Node*> args) const;

    /**
     * @brief Emit an indexer AST node (array[index]).
     *
     * @param array Array expression (owned by the returned node on success).
     * @param index Index expression (owned by the returned node on success).
     * @return Newly allocated indexer node, or nullptr if array/index are null.
     */
    Node* EmitIndexer(Node* array, Node* index) const;

    /**
     * @brief Build the expression that feeds a given input pin.
     *
     * This follows the data link feeding the pin and compiles the upstream node.
     * If the pin is not connected, returns a safe default value for the pin type.
     *
     * @param inputPin Input pin to evaluate.
     * @return Newly allocated AST node representing the value.
     */
    Node* BuildExpr(const Pin& inputPin);

    /**
     * @brief Compile a visual node (or one of its outputs) into an AST node.
     *
     * @param n Node instance.
     * @param outPinIdx Which output pin is being requested (used by compileOutput).
     * @return Newly allocated AST node, or nullptr on failure.
     */
    Node* BuildNode(const VisualNode& n, int outPinIdx = 0);

    /**
     * @brief Convert an array literal string like "[a, b, c]" into an AST Array node.
     *
     * @param text String containing an array literal.
     * @return Newly allocated AST array node.
     */
    Node* BuildArrayLiteralNode(const std::string& text) const;

    /**
     * @brief Append statements by walking a flow chain starting at a flow output pin.
     *
     * This follows execution wires and appends compiled statements into `targetScope`.
     * Flow-control nodes (if/loop/sequence/etc.) handle their own sub-scopes via
     * their descriptor `compileFlow` callbacks.
     *
     * @param flowOutputPinId Flow output pin id to start from.
     * @param targetScope AST Scope node to append statements into.
     */
    void AppendFlowChainFromOutput(ed::PinId flowOutputPinId, Node* targetScope);

    /**
     * @brief Mark entry into a loop body while compiling flow.
     *
     * Some nodes (like Loop) have outputs that are only valid inside their body.
     * These helpers let compile callbacks implement that rule.
     *
     * @param nodeIdKey Unique key for the loop node instance.
     */
    void PushActiveLoopBodyNodeId(uintptr_t nodeIdKey);

    /** @brief Mark exit from a loop body. @param nodeIdKey Unique key for the loop node instance. */
    void PopActiveLoopBodyNodeId(uintptr_t nodeIdKey);

    /** @brief Mark entry into a ForEach body. @param nodeIdKey Unique key for the ForEach node instance. */
    void PushActiveForEachBodyNodeId(uintptr_t nodeIdKey);

    /** @brief Mark exit from a ForEach body. @param nodeIdKey Unique key for the ForEach node instance. */
    void PopActiveForEachBodyNodeId(uintptr_t nodeIdKey);

    /** @brief Check whether we are currently compiling inside a loop body. @param nodeIdKey Unique key. */
    bool IsActiveLoopBodyNodeId(uintptr_t nodeIdKey) const;

    /** @brief Check whether we are currently compiling inside a ForEach body. @param nodeIdKey Unique key. */
    bool IsActiveForEachBodyNodeId(uintptr_t nodeIdKey) const;

    /**
     * @brief Reset internal flow-walk caches so an independent flow chain can be compiled.
     *
     * Used when compiling things that have their own flow entry point (for example,
     * user function bodies).
     */
    void ResetFlowTraversalState();

    /**
     * @brief Declare a graph variable name once (used by prelude callbacks).
     *
     * @param varName Variable name.
     * @return True if this name was not declared yet; false if it was already declared.
     */
    bool TryDeclareGraphVariable(const std::string& varName);

    /**
     * @brief Find a user-defined Function node by its Name field.
     *
     * @param name Function name.
     * @return Pointer to the matching VisualNode, or nullptr.
     */
    const VisualNode* FindUserFunctionByName(const std::string& name) const;

private:
    PinResolver resolver_;  ///< Fast pin connection lookup helper.

    // Execution-flow helpers (walk from Start through connected flow pins).
    void CollectFlowReachableFromOutput(ed::PinId flowOutputPinId);
    void AppendFlowNode(const VisualNode& n, int triggeredInputPinIdx, Node* targetScope);
    bool NodeRequiresFlow(const VisualNode& n) const;
    const VisualNode* FindFirstNode(NodeType type) const;

    std::string errorMsg_;               ///< Last compile error.

    // Guard sets used to detect cycles while building graph/flow AST.
    std::unordered_set<uintptr_t> activeNodeBuilds_;
    std::unordered_set<uintptr_t> activeLoopBodyNodeIds_;
    std::unordered_set<uintptr_t> activeForEachBodyNodeIds_;
    std::unordered_set<uintptr_t> flowReachableNodes_;
    std::unordered_set<uintptr_t> activeFlowOutputs_;
    std::unordered_set<uintptr_t> visitedFlowOutputs_;

    std::unordered_set<std::string> declaredGraphVariables_;

    const std::vector<VisualNode>* nodes_ = nullptr;
};
