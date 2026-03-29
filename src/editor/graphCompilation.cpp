#include "graphCompilation.h"
#include "graphState.h"
#include "../core/compiler/graphCompiler.h"
#include "VM.h"
#include "Parser/Node.h"
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

// Pimpl implementation details
class GraphCompilation::Impl
{
public:
    std::unique_ptr<VM> vm;

    /**
     * @brief Initializes the Pimpl implementation with a VM instance.
     *
     * @author Jenny
     */
    Impl()
    {
        vm = std::make_unique<VM>();
    }

    ~Impl() = default;
};

/**
 * @brief Constructs a GraphCompilation instance and initializes the implementation.
 *
 * @author Jenny
 */
GraphCompilation::GraphCompilation()
    : m_impl(std::make_unique<Impl>())
{
}

/**
 * @brief Destroys the GraphCompilation instance.
 *
 * @author Jenny
 */
GraphCompilation::~GraphCompilation() = default;

void GraphCompilation::SetLogSink(LogSink sink)
{
    m_logSink = std::move(sink);
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

    // Debug Print is optional: graphs may be valid without explicit output nodes.
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

    // Compile visual graph to AST
    GraphCompiler gc;
    Node* ast = gc.Compile(nodes, links);

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

    // Clear previous graph unit from VM
    m_impl->vm->RemoveUnit(kCompileUnitName);

    // Compile AST to VM bytecode
    m_impl->vm->CompileAST(kCompileUnitName, ast);

    // Execute the compiled graph function
    constexpr const char* kRunGraphScript = "__graph__();";
    void* printHandle = m_impl->vm->CompileTemporary(kRunGraphScript);
    if (!printHandle || !m_impl->vm->WaitForResult(printHandle))
    {
        const std::string status = "[ERROR] Runtime error: Failed to execute compiled graph\n";
        if (m_logSink)
        {
            m_logSink(status);
        }
        return { false, status };
    }
    else
    {
        const std::string status = "[OK] Compiled and executed successfully\n";
        if (m_logSink)
        {
            m_logSink(status);
        }
        return { true, status };
    }
}

