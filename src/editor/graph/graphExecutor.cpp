#include "graphExecutor.h"
#include "graphState.h"
#include "core/compiler/graphCompiler.h"
#include "core/graphModel/visualNodeUtils.h"
#include "VM.h"
#include "Parser/Node.h"
#include "../../../Demo/previewNatives.h"
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

// Run pipeline (CompileGraphSnapshot):
// A) Build AST from the visual graph  (GraphCompiler — see core/compiler/graphCompiler.h)
// B) Remove the previous compiled unit from the VM
// C) Compile AST into VM bytecode
// D) Execute the generated __graph__() entrypoint
class GraphExecutor::Impl
{
public:
    std::mutex vmMutex;
    std::shared_ptr<VM> vm;

    Impl()
    {
        vm = std::make_shared<VM>();
    }

    ~Impl() = default;
};

GraphExecutor::GraphExecutor()
    : m_impl(std::make_unique<Impl>())
{
    // Provide stop flag to demo/runtime natives so __emi_* helpers can stop
    // long-running graphs (e.g., Ticker) even while sleeping.
    SetGraphForceStopFlag(&m_forceStopRequested);
}

GraphExecutor::~GraphExecutor() = default;

void GraphExecutor::SetLogSink(LogSink sink)
{
    m_logSink = std::move(sink);
}

void GraphExecutor::RequestForceStop()
{
    m_forceStopRequested.store(true);

    // VM::Interrupt() is currently a no-op; force stop is implemented by
    // recreating the VM so runner threads are torn down in ~VM().
    if (!m_impl)
        return;

    std::lock_guard<std::mutex> lock(m_impl->vmMutex);
    m_impl->vm = std::make_shared<VM>();
}

void GraphExecutor::ClearForceStopRequest()
{
    m_forceStopRequested.store(false);
}

void GraphExecutor::CompileGraph(GraphState& state, bool resultOnly)
{
    const CompileResult result = CompileGraphSnapshot(state.GetNodes(), state.GetLinks(), resultOnly);
    state.SetCompileStatus(result.success, result.message);
}

GraphExecutor::CompileResult GraphExecutor::CompileGraphSnapshot(
    const std::vector<VisualNode>& nodes,
    const std::vector<Link>& links,
    bool resultOnly)
{
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

    std::shared_ptr<VM> vm;
    if (m_impl)
    {
        std::lock_guard<std::mutex> lock(m_impl->vmMutex);
        vm = m_impl->vm;
    }

    if (!vm)
    {
        const std::string status = "[ERROR] Error: EMI environment not initialized\n";
        if (m_logSink)
        {
            m_logSink(status);
        }
        return { false, status };
    }

    // Validate function nodes before compiling (no duplicate names for function definitions)
    {
        std::unordered_map<std::string, unsigned long long> firstByName;

        for (const VisualNode& n : nodes)
        {
            if (!n.alive || n.nodeType != NodeType::Function)
                continue;

            const std::string* fnName = FindField(n, "Name");
            const std::string name = fnName ? *fnName : std::string();
            const unsigned long long rawId = (unsigned long long)n.id.Get();

            if (name.empty())
            {
                const std::string status = "[ERROR] Compile error: Function node has an empty Name field (node id "
                    + std::to_string(rawId) + ")\n";
                if (m_logSink)
                    m_logSink(status);
                return { false, status };
            }

            auto [it, inserted] = firstByName.emplace(name, rawId);
            if (!inserted)
            {
                const std::string status = "[ERROR] Compile error: Duplicate Function name '" + name
                    + "' (node ids " + std::to_string(it->second) + " and " + std::to_string(rawId)
                    + "). Function names must be unique.\n";
                if (m_logSink)
                    m_logSink(status);
                return { false, status };
            }
        }
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

    // Step A: build AST from the current graph snapshot.
    // GraphCompiler runs its own 5-stage pipeline internally (see graphCompiler.cpp).
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

    // Step B: remove previous graph unit so reruns use only fresh code.
    vm->RemoveUnit(kCompileUnitName);

    // Step C: compile AST into VM bytecode.
    vm->CompileAST(kCompileUnitName, ast);

    if (m_forceStopRequested.load())
    {
        return makeCancelled();
    }

    // Step D: execute generated graph entrypoint.
    constexpr const char* kRunGraphScript = "__graph__();";
    void* printHandle = vm->CompileTemporary(kRunGraphScript);
    if (!printHandle || !vm->WaitForResult(printHandle))
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
