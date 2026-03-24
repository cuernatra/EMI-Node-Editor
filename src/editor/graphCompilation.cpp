#include "graphCompilation.h"
#include "graphState.h"
#include "../core/compiler/graphCompiler.h"
#include "VM.h"
#include "Parser/Node.h"
#include <iostream>
#include <sstream>
#include <string>

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

/**
 * @brief Compiles and executes a visual graph.
 *
 * @param state The graph state containing nodes and links to compile.
 * @param resultOnly If true, suppress debug output; if false, enable debug logging.
 *
 * @author Jenny
 */
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
        state.SetCompileStatus(false, "✗ Error: EMI environment not initialized");
        return;
    }

    // Validate graph has a Debug Print node for output
    if (!state.HasOutputNode())
    {
        state.SetCompileStatus(false, "✗ Error: Graph requires a Debug Print node for output");
        return;
    }

    // Compile visual graph to AST
    GraphCompiler gc;
    Node* ast = gc.Compile(state.GetNodes(), state.GetLinks());

    if (gc.HasError || !ast)
    {
        state.SetCompileStatus(false, "✗ Compile error: " + gc.GetError());
        if (ast) delete ast;
        return;
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
    if (!m_impl->vm->WaitForResult(printHandle))
    {
        state.SetCompileStatus(false, "✗ Runtime error: Failed to execute compiled graph");
        return;
    }

    if (resultOnly)
        state.SetCompileStatus(true, "✓ Compilation successful");
    else
        state.SetCompileStatus(true, "✓ Compiled and executed successfully");
}

