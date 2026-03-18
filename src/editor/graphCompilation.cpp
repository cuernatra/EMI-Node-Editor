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

void GraphCompilation::CompileGraph(GraphState& state)
{
    if (!m_impl || !m_impl->vm)
    {
        state.SetCompileStatus(false, "Error: EMI environment not initialized");
        return;
    }

    // Pre-validate: graph should have at least a Start node or Output/Function sink.
    // Both are now optional: Start nodes drive flow execution, Output captures return value.
    const bool hasStartNode = state.HasNodeType(NodeType::Start);
    const bool hasOutputNode = state.HasOutputNode();
    if (!hasStartNode && !hasOutputNode)
    {
        state.SetCompileStatus(false, "Error: graph must have either a Start node or Output node.");
        return;
    }

    // Step 1: Compile visual graph to AST using entrypoint-driven statement pass
    // (respects Start nodes as explicit entry points and builds flow chains from there).
    GraphCompiler gc;
    Node* ast = gc.Compile(state.GetNodes(), state.GetLinks());

    if (gc.HasError || !ast)
    {
        state.SetCompileStatus(false, "Compile error: " + gc.GetError());
        if (ast) delete ast;
        return;
    }

    // Step 2: Print AST to console for debugging (only available in DEBUG builds).
    std::cout << "\n=== Generated AST ===\n";
#ifdef DEBUG
    ast->print("");
#endif
    std::cout << "=== End AST ===\n\n";

    constexpr const char* kCompileUnitName = "__graph_unit__";

    // Remove previous graph unit so repeated compile runs don't accumulate stale symbols.
    m_impl->vm->RemoveUnit(kCompileUnitName);

    ast->print("");

    // Step 3: Compile AST directly via EMI's internal AST walker.
    // CompileAST enqueues parsing and blocks until completed.
    // VM takes ownership of ast pointer after this call.
    m_impl->vm->CompileAST(kCompileUnitName, ast);

    // Step 4: Resolve and execute generated entry-point function.
    void* rawFunction = m_impl->vm->GetFunctionID(GraphCompiler::kGraphFunctionName);
    if (!rawFunction)
    {
        state.SetCompileStatus(false, "Runtime error: compiled function '__graph__' not found in VM");
        return;
    }

    FunctionHandle fn(rawFunction, nullptr);
    const std::span<InternalValue> noArgs;
    size_t returnSlot = m_impl->vm->CallFunction(fn, noArgs);
    if (returnSlot == static_cast<size_t>(-1))
    {
        state.SetCompileStatus(false, "Runtime error: failed to invoke '__graph__'");
        return;
    }

    // Step 5: Read and format return value.
    InternalValue result = m_impl->vm->GetReturnValue(returnSlot);

    std::ostringstream msg;
    msg << "OK — compiled and executed (__graph__ returned ";
    switch (result.getType())
    {
        case ValueType::Number:
            msg << result.as<double>();
            break;
        case ValueType::Boolean:
            msg << (result.as<bool>() ? "true" : "false");
            break;
        case ValueType::String:
        {
            const char* text = result.as<const char*>();
            msg << (text ? text : "");
            break;
        }
        case ValueType::External:
            msg << "<external>";
            break;
        case ValueType::Undefined:
        default:
            msg << "undefined";
            break;
    }
    msg << ")";
    state.SetCompileStatus(true, msg.str());
}

