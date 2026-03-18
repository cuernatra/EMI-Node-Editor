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
#include <unordered_map>
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

    // Implementation methods called by descriptor lambdas in nodeRegistry.cpp
    Node* BuildConstant  (const VisualNode& n);   ///< Builds AST for Constant nodes
    Node* BuildOperator  (const VisualNode& n);   ///< Builds AST for Operator nodes
    Node* BuildComparison(const VisualNode& n);   ///< Builds AST for Comparison nodes
    Node* BuildLogic     (const VisualNode& n);   ///< Builds AST for Logic nodes
    Node* BuildSequence  (const VisualNode& n);   ///< Builds AST for Sequence nodes
    Node* BuildBranch    (const VisualNode& n);   ///< Builds AST for Branch nodes
    Node* BuildLoop      (const VisualNode& n);   ///< Builds AST for Loop nodes
    Node* BuildStart     (const VisualNode& n);   ///< Builds AST for Start nodes (entrypoint flow)
    Node* BuildVariable  (const VisualNode& n);   ///< Builds AST for Variable nodes
    Node* BuildOutput    (const VisualNode& n);   ///< Builds AST for Output nodes
    Node* BuildFunctionCall(const VisualNode& n);   ///< Builds AST for FunctionCall nodes (named call with N args + result)
    Node* BuildPrint     (const VisualNode& n);   ///< Builds AST for Print nodes (println)
    Node* BuildDelay     (const VisualNode& n);   ///< Builds AST for Delay nodes (delay ms)
    Node* BuildRandomRange(const VisualNode& n);  ///< Builds AST for RandomRange nodes (random min max)
    Node* BuildDoOnce    (const VisualNode& n);   ///< Builds AST for DoOnce flow nodes
    Node* BuildGate      (const VisualNode& n);   ///< Builds AST for Gate flow nodes
    Node* BuildFunction  (const VisualNode& n);   ///< Builds AST for Function nodes

private:
    PinResolver resolver_;  ///< Analyzes pin connections and resolves input sources

    Node* BuildExpr(const Pin& inputPin);        ///< Recursively builds AST expression from a pin's connected output
    Node* BuildNode(const VisualNode& n, int outPinIdx = 0);  ///< Builds AST node by looking up and invoking descriptor's compile callback
    Node* BuildFlowChainFromNode(const VisualNode& node);      ///< Builds a statement node and follows default flow continuation
    Node* BuildFlowChainFromPin(const Pin& outFlowPin);        ///< Builds statement chain connected to a flow output pin

    const Pin* GetOutputPinByName(const VisualNode& n, const char* name) const;  ///< Finds an output pin by name
    const VisualNode* FindFlowTargetNode(const Pin& outFlowPin) const;            ///< Resolves the node connected to a flow output pin
    bool HasIncomingFlow(const VisualNode& n) const;                               ///< True if any flow input pin is linked

    Node* MakeNode(Token t)                    const;  ///< Creates a bare AST node with the given token type
    Node* MakeNumberNode(double v)             const;  ///< Creates an AST numeric literal node
    Node* MakeBoolNode(bool v)                 const;  ///< Creates an AST boolean literal node
    Node* MakeStringNode(const std::string& s) const;  ///< Creates an AST string literal node
    Node* MakeIdNode(const std::string& name)  const;  ///< Creates an AST identifier node

    const std::string* GetField(const VisualNode& n, const std::string& name) const;  ///< Retrieves a field value from a visual node's data
    const Pin* GetInputPinByName(const VisualNode& n, const char* name) const;        ///< Finds an input pin by name

    static Token OperatorToken(const std::string& op);  ///< Maps operator strings (+, -, etc.) to Token enum values
    static Token CompareToken (const std::string& op);  ///< Maps comparison strings (==, <, etc.) to Token enum values
    static Token LogicToken   (const std::string& op);  ///< Maps logic strings (AND, OR, etc.) to Token enum values

    void Error(const std::string& msg);  ///< Records a compilation error message
    std::string errorMsg_;               ///< Stores the last compilation error

    // Tracks nodes currently being built to detect recursive cycles.
    std::unordered_set<uintptr_t> activeNodeBuilds_;

    // Per-compilation caches for flow traversal.
    const std::vector<Link>* links_ = nullptr;
    std::unordered_map<ed::PinId, const Pin*> pinsById_;
};
