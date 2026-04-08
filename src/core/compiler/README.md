# core/compiler/

Turns a visual graph (boxes + wires) into an AST the VM can run.

You normally do not edit this folder when adding nodes.
Work in `registry/nodes/` instead, using the helpers here as tools.

## Files

| File | What it does |
|---|---|
| `graphCompiler.h/.cpp` | Main compiler — entry point is `GraphCompiler::Compile()` |
| `pinResolver.h/.cpp` | Builds fast lookups: "what feeds this pin?" / "where does this wire go?" |
| `nodeCompileHelpers.h` | Helper toolbox included by all node files |
| `astBuilders.h` | Low-level AST node constructors (`MakeNode`, `MakeBinaryOpNode`, etc.) |
| `textParseHelpers.h` | String utilities: `TrimCopy`, `TryParseDoubleExact`, `SplitArrayItemsTopLevel` |

## What the compiler does

Five stages, in order:

1. **Index links** — build fast pin → node lookup tables (`PinResolver`)
2. **Mark reachable nodes** — walk execution wires from Start; used to avoid reading stale values
3. **Prelude** — run `compilePrelude` callbacks to emit variable/temp declarations
4. **Flow chain** — walk execution wires from Start, compile each node into statements
5. **Top-level defs** — run `compileTopLevel` callbacks (e.g. user-defined functions)

## How to add a flow-control node

Flow-control nodes (Branch, Loop, Sequence, …) use `compileFlow` instead of `compile`.
The difference: **you must call `AppendFlowChainFromOutput` on every connected flow output pin.**
If you forget, execution silently stops — the compiler asserts in debug builds if it detects this.

```cpp
void CompileMyBranchNode(GraphCompiler* compiler, const VisualNode& n, Node* targetScope)
{
    const Pin* condPin = FindInputPin(n, "Condition");
    if (!condPin) { compiler->Error("Branch needs Condition"); return; }

    Node* ifNode = MakeNode(Token::If);
    ifNode->children.push_back(compiler->BuildExpr(*condPin));

    // True branch — create a sub-scope and fill it by following the "True" wire.
    Node* trueScope = MakeNode(Token::Scope);
    ifNode->children.push_back(trueScope);
    const Pin* truePin = FindOutputPin(n, "True");
    if (truePin)
        compiler->AppendFlowChainFromOutput(truePin->id, trueScope);

    if (compiler->HasError) { delete ifNode; return; }

    targetScope->children.push_back(ifNode);

    // Continue the main execution chain after this node.
    const Pin* outPin = FindOutputPin(n, "Out");
    if (outPin)
        compiler->AppendFlowChainFromOutput(outPin->id, targetScope);
}
```

## When to edit this folder

- Adding a helper that many nodes need (add to `nodeCompileHelpers.h`).
- Changing a compiler-wide rule (e.g. flow-reachability guard, cycle detection).
- Adding a new compilation stage.

Do not add node-type-specific cases inside `graphCompiler.cpp`.
