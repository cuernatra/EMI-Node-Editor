#include "graphCompilation.h"
#include "graphState.h"
#include "core/compiler/graphCompiler.h"
#include "VM.h"
#include "Parser/Node.h"
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

// Compile pipeline in plain terms:
// 1) Convert node graph to AST.
// 2) Compile AST into VM bytecode.
// 3) Run generated entrypoint `__graph__()`.
class GraphCompilation::Impl
{
public:
    std::unique_ptr<VM> vm;

    Impl()
    {
        vm = std::make_unique<VM>();
    }

    ~Impl() = default;
};

GraphCompilation::GraphCompilation()
    : m_impl(std::make_unique<Impl>())
{
}

GraphCompilation::~GraphCompilation() = default;

void GraphCompilation::SetLogSink(LogSink sink)
{
    m_logSink = std::move(sink);
}

void GraphCompilation::RequestForceStop()
{
    m_forceStopRequested.store(true);
    if (m_impl && m_impl->vm)
    {
        m_impl->vm->Interrupt();
    }
}

void GraphCompilation::ClearForceStopRequest()
{
    m_forceStopRequested.store(false);
    if (m_impl && m_impl->vm)
    {
        m_impl->vm->ClearInterrupt();
    }
}

void GraphCompilation::CompileGraph(GraphState& state, bool resultOnly)
{
    const CompileResult result = CompileGraphSnapshot(state.GetNodes(), state.GetLinks(), resultOnly);
    state.SetCompileStatus(result.success, result.message);
}

GraphCompilation::CompileResult GraphCompilation::CompileGraphSnapshot(
    const std::vector<VisualNode>& nodes,
    const std::vector<Link>& links,
    bool resultOnly)
{
    if (m_impl && m_impl->vm)
    {
        m_impl->vm->ClearInterrupt();
    }

    auto makeCancelled = [&]() -> CompileResult
    {
        const std::string status = "[WARN] Compile force-stopped by user\n";
        if (m_logSink)
        {
            m_logSink(status);
        }
        return { false, status };
    };

    if (m_logSink)
    {
        m_logSink("Compiling graph...\n");
    }

    if (resultOnly)
    {
        EMI::SetCompileLogLevel(EMI::LogLevel::Warning);
        EMI::SetRuntimeLogLevel(EMI::LogLevel::Warning);
        EMI::SetScriptLogLevel(EMI::LogLevel::Info);
    }
    else
    {
        EMI::SetCompileLogLevel(EMI::LogLevel::Debug);
        EMI::SetRuntimeLogLevel(EMI::LogLevel::Debug);
        EMI::SetScriptLogLevel(EMI::LogLevel::Debug);
    }

    if (!m_impl || !m_impl->vm)
    {
        const std::string status = "[ERROR] Error: EMI environment not initialized\n";
        if (m_logSink)
        {
            m_logSink(status);
        }
        return { false, status };
    }

    if (m_forceStopRequested.load())
    {
        return makeCancelled();
    }

    // Output nodes are optional. We keep a warning because users often expect console output.
    bool hasOutputNode = false;
    for (const auto& n : nodes)
    {
        if (n.alive && n.nodeType == NodeType::Output)
        {
            hasOutputNode = true;
            break;
        }
    }

    if (!hasOutputNode && m_logSink)
    {
        m_logSink("[WARN] No Debug Print node found. Graph will run without console output.\n");
    }

    // Stage 1: build AST from the current graph snapshot.
    GraphCompiler gc;
    Node* ast = gc.Compile(nodes, links);

    if (m_forceStopRequested.load())
    {
        if (ast) delete ast;
        return makeCancelled();
    }

    if (gc.HasError || !ast)
    {
        const std::string status = "[ERROR] Compile error: " + gc.GetError() + "\n";
        if (m_logSink)
        {
            m_logSink(status);
        }
        if (ast) delete ast;
        return { false, status };
    }

    if (!resultOnly)
    {
#ifdef DEBUG
        ast->print("");
#endif
        std::cout << "\n";
    }

    constexpr const char* kCompileUnitName = "__graph_unit__";

    // Stage 2: remove previous graph unit so reruns use only fresh code.
    m_impl->vm->RemoveUnit(kCompileUnitName);

    // Stage 3: compile AST into VM bytecode.
    m_impl->vm->CompileAST(kCompileUnitName, ast);

    if (m_forceStopRequested.load())
    {
        return makeCancelled();
    }

    // Stage 4: execute generated graph entrypoint.
    constexpr const char* kRunGraphScript = "__graph__();";
    void* printHandle = m_impl->vm->CompileTemporary(kRunGraphScript);
    if (!printHandle || !m_impl->vm->WaitForResult(printHandle))
    {
        if (m_forceStopRequested.load())
        {
            return makeCancelled();
        }

        const std::string status = "[ERROR] Runtime error: Failed to execute compiled graph\n";
        if (m_logSink)
        {
            m_logSink(status);
        }
        return { false, status };
    }
    else
    {
        if (m_forceStopRequested.load())
        {
            return makeCancelled();
        }

        const std::string status = "[OK] Compiled and executed successfully\n";
        if (m_logSink)
        {
            m_logSink(status);
        }
        return { true, status };
    }
}

