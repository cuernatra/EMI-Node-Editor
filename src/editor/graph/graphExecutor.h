/** @file graphExecutor.h */
/**
 * @brief Drives the full graph run: build AST → compile bytecode → execute.
 *
 * This is the bridge between the editor (GraphState) and the core compiler
 * (GraphCompiler) + VM. It is the right place to start when debugging what
 * happens when the user presses Run.
 *
 * For the graph → AST step specifically, see core/compiler/graphCompiler.h.
 */

#pragma once

#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <atomic>

class GraphState;
struct VisualNode;
struct Link;

/** @brief Orchestrates graph compilation and VM execution. */
class GraphExecutor
{
public:
    using LogSink = std::function<void(const std::string&)>;

    struct CompileResult
    {
        bool success = false;
        std::string message;
    };

    /** @brief Initialize compilation runtime resources. */
    GraphExecutor();

    /** @brief Release compilation runtime resources. */
    ~GraphExecutor();

    /** @brief Compile and execute using mutable editor state. */
    void CompileGraph(GraphState& state, bool resultOnly = false);

    /// Compile and execute from immutable graph snapshots (thread-friendly API).
    CompileResult CompileGraphSnapshot(const std::vector<VisualNode>& nodes,
                                       const std::vector<Link>& links,
                                       bool resultOnly = false);

    /// Set optional sink for forwarding compile status messages to external UI.
    void SetLogSink(LogSink sink);

    /// Request force-stop for currently running compile/execute operation.
    void RequestForceStop();

    /// Clear any previously requested force-stop state before a new compile run.
    void ClearForceStopRequest();

private:
    // Pimpl to hide EMI types from header
    class Impl;
    std::unique_ptr<Impl> m_impl;
    LogSink m_logSink;
    std::atomic<bool> m_forceStopRequested{ false };
};
