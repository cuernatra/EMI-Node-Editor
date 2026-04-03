/**
 * @file graphCompiler.h
 * @brief Graph-to-AST compilation for EMI-Script execution
 * 
 * Converts visual node graphs to EMI-Script AST (Abstract Syntax Tree)
 * for compilation and execution via the EMI runtime.
 * 
 * Compilation flow:
 * 1. PinResolver analyzes graph connectivity and maps input pins to sources
 * 2. GraphCompiler recursively builds AST nodes from visual nodes
 * 3. Generated AST is wrapped in a function and returned to caller
 * TODO: 4. AST will be compiled via EMI's Parser::ParseAST with CompileOptions::Ptr
 * TODO: 5. VM will execute with result recoverable via GetReturnValue
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
 * @brief Compiles visual graphs to EMI-Script AST
 * 
 * Translates connected node graphs into an Abstract Syntax Tree suitable
 * for EMI-Script compilation. Each visual node is converted to one or more AST
 * nodes based on its type (operators become expressions, prints become
 * function calls, etc.).
 * 
 * The generated AST is wrapped in a function named kGraphFunctionName
 * so that future EMI integration can execute the graph and recover its return value.
 * 
 * @author Jenny 
 */
class GraphCompiler
{
public:
    bool HasError = false;  ///< Set to true if Compile() fails

    /**
     * The name of the generated wrapper function inside the VM
     * @brief Global function name constant used as the entry point for compiled graph execution.
     * @details This constant defines the internal function name that serves as the main entry point
     * when the visual programming graph is compiled into executable code. All graph logic is wrapped
     * and executed through this function during runtime.
     */
    static constexpr const char* kGraphFunctionName = "__graph__";

    /**
     * @brief Compile visual graph to EMI-Script AST
     * @param nodes All nodes in the graph (order doesn't matter)
     * @param links All connections between pins
     * @return Root Scope Node* wrapping a function declaration, or nullptr on error
     * 
     * On success:
     * - Returns a complete AST ready for compilation
     * - HasError is false
     * - Caller owns the AST and must call delete when done
     * - TODO: Future integration will pass AST to EMI's Parser::ParseAST,
     *   which will take ownership of the AST pointer
     * 
     * On failure:
     * - Returns nullptr (or incomplete AST that must be deleted)
     * - HasError is true
     * - GetError() contains diagnostic message
     * - Caller must delete the returned AST if non-nullptr
     * 
     * The AST represents a function containing the graph logic, with the
     * Output node's expression becoming a return statement.
     */
    Node* Compile(const std::vector<VisualNode>& nodes,
                  const std::vector<Link>&       links);

    /**
     * @brief Get the last compilation error message
     * @return User-facing diagnostic message (empty if no error)
     */
    const std::string& GetError() const { return errorMsg_; }

    void Error(const std::string& msg);  ///< Records a compilation error message

    /**
     * @brief Resolve whether an input pin is connected.
     * @param inputPin Input pin to inspect
     * @return PinSource when connected, or nullptr when unconnected
     */
    const PinSource* Resolve(const Pin& inputPin) const { return resolver_.Resolve(inputPin.id); }

    /**
     * @brief Resolve downstream flow target for a flow output pin.
     * @param outputPin Flow output pin to inspect
     * @return FlowTarget when connected, or nullptr when unconnected
     */
    const FlowTarget* ResolveFlow(const Pin& outputPin) const { return resolver_.ResolveFlow(outputPin.id); }

    // Expression primitives used directly by descriptor lambdas.
    Node* EmitBinaryOp   (Token op, Node* lhs, Node* rhs) const;
    Node* EmitUnaryOp    (Token op, Node* operand) const;
    Node* EmitFunctionCall(const std::string& name, std::vector<Node*> args) const;
    Node* EmitIndexer    (Node* array, Node* index) const;

    Node* BuildExpr(const Pin& inputPin);        ///< Recursively builds AST expression from a pin's connected output
    Node* BuildNode(const VisualNode& n, int outPinIdx = 0);  ///< Builds AST node by looking up and invoking descriptor's compile callback
    Node* BuildArrayLiteralNode(const std::string& text) const; ///< Parses "[a, b, ...]" text into AST array literal

private:
    PinResolver resolver_;  ///< Analyzes pin connections and resolves input sources

    // Flow compilation helpers (Start-rooted execution order)
    void CollectFlowReachableFromOutput(ed::PinId flowOutputPinId);
    void AppendFlowChainFromOutput(ed::PinId flowOutputPinId, Node* targetScope);
    void AppendFlowNode(const VisualNode& n, int triggeredInputPinIdx, Node* targetScope);

    bool NodeRequiresFlow(const VisualNode& n) const;
    const VisualNode* FindFirstNode(NodeType type) const;
    const Pin* GetOutputPinByName(const VisualNode& n, const char* name) const;

    Node* MakeNode(Token t)                    const;  ///< Creates a bare AST node with the given token type
    Node* MakeNumberNode(double v)             const;  ///< Creates an AST numeric literal node
    Node* MakeBoolNode(bool v)                 const;  ///< Creates an AST boolean literal node
    Node* MakeStringNode(const std::string& s) const;  ///< Creates an AST string literal node
    Node* MakeIdNode(const std::string& name)  const;  ///< Creates an AST identifier node

    std::string errorMsg_;               ///< Stores the last compilation error

    // Tracks nodes currently being built to detect recursive cycles.
    std::unordered_set<uintptr_t> activeNodeBuilds_;
    std::unordered_set<uintptr_t> activeLoopBodyNodeIds_;
    std::unordered_set<uintptr_t> activeForEachBodyNodeIds_;
    std::unordered_set<uintptr_t> flowReachableNodes_;
    std::unordered_set<uintptr_t> activeFlowOutputs_;
    std::unordered_set<uintptr_t> visitedFlowOutputs_;

    const std::vector<VisualNode>* nodes_ = nullptr;
};
