/** @file graphCompilation.h */
/** @brief Compile and execute graph snapshots. */

#pragma once

#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <atomic>

class GraphState;
struct VisualNode;
struct Link;

/** @brief Bridges editor graph data to compiler/VM execution. */
class GraphCompilation
{
public:
    using LogSink = std::function<void(const std::string&)>;

    struct CompileResult
    {
        bool success = false;
        std::string message;
    };

    /** @brief Initialize compilation runtime resources. */
    GraphCompilation();

    /** @brief Release compilation runtime resources. */
    ~GraphCompilation();

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


