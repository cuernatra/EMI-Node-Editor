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

    // Pre-validate: check for Output node
    if (!state.HasOutputNode())
    {
        state.SetCompileStatus(false, "Error: graph has no Output node.");
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

    // Step 4: Execute and print via EMI-Script itself.
    // This runs the generated __graph__ function and routes output through
    // the script logger (terminal) using intrinsic println.
    constexpr const char* kPrintGraphResultScript = "println(__graph__());";
    void* printHandle = m_impl->vm->CompileTemporary(kPrintGraphResultScript);
    if (!m_impl->vm->WaitForResult(printHandle))
    {
        state.SetCompileStatus(false, "Runtime error: failed to execute 'println(__graph__())'");
        return;
    }

    if (resultOnly)
        state.SetCompileStatus(true, "OK");
    else
        state.SetCompileStatus(true, "OK — compiled and executed (__graph__ printed via EMI-Script println)");
}

