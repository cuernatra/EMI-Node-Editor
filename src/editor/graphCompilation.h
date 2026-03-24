/**
 * @file graphCompilation.h
 * @brief Graph-to-code compilation bridge
 * 
 * Provides high-level interface for compiling the visual node graph
 * into executable EMI-Script code. Acts as a bridge between the editor
 * and the core GraphCompiler, managing EMI environment and execution.
 * 
 */

#pragma once

#include <memory>
#include <functional>
#include <string>

class GraphState;

/**
 * @brief High-level graph compilation and execution interface
 * 
 * Manages the EMI-Script VM environment and provides a simple compilation
 * interface for the editor. Validates the graph, compiles to AST, and
 * prepares for EMI's internal ASTWalker compilation.
 * 
 * Currently creates and prints the AST. Future integration will use
 * EMI's Parser::ParseAST() and internal execution APIs.
 * 
 */
class GraphCompilation
{
public:
    using LogSink = std::function<void(const std::string&)>;

    /**
     * @brief Initialize the EMI environment for compilation
     * 
     * Creates an EMI::VMHandle for script compilation and execution.
     * Must be called before CompileGraph().
     */
    GraphCompilation();

    /**
     * @brief Clean up the EMI environment
     */
    ~GraphCompilation();

    /**
     * @brief Compile and execute the graph
     * @param state The graph state to compile and execute; updated with results
     * 
     * Performs the following steps:
     * 1. Pre-validate graph (check for Debug Print node)
     * 2. Compile visual graph to EMI-Script AST via GraphCompiler
     * 3. TODO: Compile AST with EMI's internal Parser::ParseAST (ASTWalker)
     * 4. TODO: Extract and execute the __graph__ function
     * 5. TODO: Retrieve return value and update state
     * 
     * Currently creates and prints AST for debugging. Future integration
     * will use EMI's internal compilation and execution APIs.
     * All compilation and runtime errors are caught and stored
     * in the graph state for display to the user.
     */
    void CompileGraph(GraphState& state, bool resultOnly = false);

    /// Set optional sink for forwarding compile status messages to external UI.
    void SetLogSink(LogSink sink);

private:
    // Pimpl to hide EMI types from header
    class Impl;
    std::unique_ptr<Impl> m_impl;
    LogSink m_logSink;
};


