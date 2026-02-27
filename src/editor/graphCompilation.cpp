#include "graphCompilation.h"
#include "graphState.h"
#include "../core/compiler/graphCompiler.h"
#include "EMI/EMI.h"
#include "Parser/Node.h"
#include <iostream>
#include <string>

// Pimpl implementation details
class GraphCompilation::Impl
{
public:
    std::unique_ptr<EMI::VMHandle> vm;

    Impl()
    {
        vm = std::make_unique<EMI::VMHandle>(EMI::CreateEnvironment());
    }

    ~Impl() = default;
};

GraphCompilation::GraphCompilation()
    : m_impl(std::make_unique<Impl>())
{
}

GraphCompilation::~GraphCompilation() = default;

void GraphCompilation::CompileGraph(GraphState& state)
{
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

    // Step 2: Print AST to console for debugging
    std::cout << "\n=== Generated AST ===\n";
    ast->print("");
    std::cout << "=== End AST ===\n\n";

    // Step 3: TODO - Compile AST directly with EMI's internal VM
    // This would use EMI's internal Parser::ParseAST() with CompileOptions::Ptr
    // pointing to our generated AST, triggering ASTWalker internally
    // to generate bytecode without text serialization.
    // Code would look like:
    //   EMI::CompileOptions options;
    //   options.Ptr = ast;
    //   EMI::Parser::ParseAST(m_impl->vm.get(), options);
    
    // Step 4: TODO - Execute the compiled function
    // Once EMI compiles the AST, retrieve the function handle and call it:
    //   auto fnHandle = m_impl->vm->GetFunctionHandle("__graph__");
    //   auto result = EMI::CallFunction(m_impl->vm.get(), fnHandle);
    //   double resultValue = result.get<double>();
    
    // For now, mark compilation as successful with AST created
    delete ast;  // Clean up AST since we're not using it yet
    state.SetCompileStatus(true, "OK — AST created (EMI internal compilation not yet integrated)");
}

