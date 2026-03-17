#include "graphCompilation.h"
#include "graphState.h"
#include "../core/compiler/graphCompiler.h"
#include "VM.h"
#include "Parser/Node.h"
#include <string>

// Pimpl implementation details
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

void GraphCompilation::CompileGraph(GraphState& state, bool resultOnly)
{
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
        state.SetCompileStatus(false, "Error: EMI environment not initialized");
        return;
    }

    // Pre-validate: check for Debug Print node
    if (!state.HasOutputNode())
    {
        state.SetCompileStatus(false, "Error: graph has no Debug Print node.");
        return;
    }

    // Step 1: Compile visual graph to AST
    GraphCompiler gc;
    Node* ast = gc.Compile(state.GetNodes(), state.GetLinks());

    if (gc.HasError || !ast)
    {
        state.SetCompileStatus(false, "Compile error: " + gc.GetError());
        if (ast) delete ast;
        return;
    }

    constexpr const char* kCompileUnitName = "__graph_unit__";

    // Remove previous graph unit so repeated compile runs don't accumulate stale symbols.
    m_impl->vm->RemoveUnit(kCompileUnitName);

    // Step 3: Compile AST directly via EMI's internal AST walker.
    // CompileAST enqueues parsing and blocks until completed.
    // VM takes ownership of ast pointer after this call.
    m_impl->vm->CompileAST(kCompileUnitName, ast);

    // Step 4: Execute compiled graph function.
    // Debug Print nodes inside the graph are now responsible for
    // terminal output, so we only invoke __graph__() here.
    constexpr const char* kRunGraphScript = "__graph__();";
    void* printHandle = m_impl->vm->CompileTemporary(kRunGraphScript);
    if (!m_impl->vm->WaitForResult(printHandle))
    {
        state.SetCompileStatus(false, "Runtime error: failed to execute '__graph__()'");
        return;
    }

    if (resultOnly)
        state.SetCompileStatus(true, "OK");
    else
        state.SetCompileStatus(true, "OK — compiled and executed (__graph__)");
}

