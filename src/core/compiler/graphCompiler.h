/** @file graphCompiler.h */
/** @brief Turns visual graph nodes into executable script AST. */

#pragma once
#include "../graph/visualNode.h"
#include "../graph/link.h"
#include "pinResolver.h"
#include "Parser/Node.h"
#include "Parser/Lexer.h"
#include <string>
#include <vector>
#include <unordered_set>

/** @brief Builds script AST from connected visual nodes. */
class GraphCompiler
{
public:
    bool HasError = false;  ///< Set to true if Compile() fails

    /** Name of the generated function used as compiled graph entrypoint. */
    static constexpr const char* kGraphFunctionName = "__graph__";

    /**
     * @brief Compile graph data into an AST tree.
     * @return Root AST node on success, or nullptr on failure.
     *
     * Caller owns the returned node and must delete it.
     */
    Node* Compile(const std::vector<VisualNode>& nodes,
                  const std::vector<Link>&       links);

    /** @brief Get last compile error text. */
    const std::string& GetError() const { return errorMsg_; }

    void Error(const std::string& msg);  ///< Records a compilation error message

    /** @brief Find what output pin feeds an input pin. */
    const PinSource* Resolve(const Pin& inputPin) const { return resolver_.Resolve(inputPin.id); }

    /** @brief Find where a flow output continues next. */
    const FlowTarget* ResolveFlow(const Pin& outputPin) const { return resolver_.ResolveFlow(outputPin.id); }

    // Small AST building helpers used by node compile callbacks.
    Node* EmitBinaryOp   (Token op, Node* lhs, Node* rhs) const;
    Node* EmitUnaryOp    (Token op, Node* operand) const;
    Node* EmitFunctionCall(const std::string& name, std::vector<Node*> args) const;
    Node* EmitIndexer    (Node* array, Node* index) const;

    Node* BuildExpr(const Pin& inputPin);        ///< Build expression tree coming into an input pin.
    Node* BuildNode(const VisualNode& n, int outPinIdx = 0);  ///< Build AST for one visual node.
    Node* BuildArrayLiteralNode(const std::string& text) const; ///< Convert "[a, b, ...]" text into array AST.

private:
    PinResolver resolver_;  ///< Fast pin connection lookup helper.

    // Execution-flow helpers (walk from Start through connected flow pins).
    void CollectFlowReachableFromOutput(ed::PinId flowOutputPinId);
    void AppendFlowChainFromOutput(ed::PinId flowOutputPinId, Node* targetScope);
    void AppendFlowNode(const VisualNode& n, int triggeredInputPinIdx, Node* targetScope);

    bool NodeRequiresFlow(const VisualNode& n) const;
    const VisualNode* FindFirstNode(NodeType type) const;
    const Pin* GetOutputPinByName(const VisualNode& n, const char* name) const;

    Node* MakeNode(Token t)                    const;  ///< Create raw AST node.
    Node* MakeNumberNode(double v)             const;  ///< Create number literal AST node.
    Node* MakeBoolNode(bool v)                 const;  ///< Create bool literal AST node.
    Node* MakeStringNode(const std::string& s) const;  ///< Create string literal AST node.
    Node* MakeIdNode(const std::string& name)  const;  ///< Create identifier AST node.

    std::string errorMsg_;               ///< Last compile error.

    // Guard sets used to detect cycles while building graph/flow AST.
    std::unordered_set<uintptr_t> activeNodeBuilds_;
    std::unordered_set<uintptr_t> activeLoopBodyNodeIds_;
    std::unordered_set<uintptr_t> activeForEachBodyNodeIds_;
    std::unordered_set<uintptr_t> flowReachableNodes_;
    std::unordered_set<uintptr_t> activeFlowOutputs_;
    std::unordered_set<uintptr_t> visitedFlowOutputs_;

    const std::vector<VisualNode>* nodes_ = nullptr;
};
