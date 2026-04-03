# Node Creation Guide

This guide explains the normal path for adding a node in this codebase.

## What to edit

For a routine node, edit:

1. `src/core/graph/types.h`
2. One file under `src/core/registry/nodes/`
3. Tests if the new node changes behavior or serialization

## File ownership

- `src/core/graph/types.h` owns the `NodeType` enum.
- `src/core/registry/nodeDescriptor.h` defines the full node structure: pins, fields, callbacks, palette variants, and save token.
- `src/core/registry/nodes/*.cpp` owns the node recipe and its compile/deserialization callbacks.
- `src/editor/*` should only be edited when the node needs special UI behavior.

## Normal node pattern

A normal node is defined by:

- `type`
- `label`
- `pins`
- `fields`
- a named compile callback
- `deserialize = nullptr` unless the node has a dynamic pin layout
- `category`
- `paletteVariants` if the same node type appears more than once in the palette
- `saveToken`

## Steps

1. Add the new `NodeType` value in `src/core/graph/types.h`.
2. Add a descriptor entry in the correct category file.
3. Write a named compile callback at the top of that same file.
4. Keep the callback readable by using helpers from `nodeCompileHelpers.h`.
5. Set `saveToken` explicitly.
6. Build and run tests.
7. Verify `saveToken` round-trips through registry lookup.

## Category map

- `eventNodes.cpp`: start and event-style nodes
- `dataNodes.cpp`: constants, variables, arrays, and output
- `logicNodes.cpp`: math, comparison, and boolean logic
- `flowNodes.cpp`: delay, sequence, branch, loop, foreach, and while
- `structNodes.cpp`: struct define and struct create

## Dynamic pin nodes

If a node has a changing pin layout, add a named deserialize callback in the same category file.

Examples of dynamic pin nodes in this project:

- Variable
- Sequence
- StructCreate

## Good rules

- Keep compile logic close to the descriptor.
- Use the existing helper functions instead of adding new compiler methods for every node.
- Keep editor changes out of normal node work.
- Make error messages clear and user-facing.

## Quick example

```cpp
Node* CompileMyNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* input = FindInputPin(n, "Value");
    if (!input)
    {
        compiler->Error("My Node needs Value input");
        return nullptr;
    }

    Node* expr = BuildNumberInput(compiler, n, *input, "Value");
    return compiler->EmitUnaryOp(Token::Not, expr);
}
```

## What should not happen

- Do not add new node-specific methods to `GraphCompiler` for routine nodes.
- Do not rely on a fallback `saveToken`.
- Do not leave `saveToken` empty and expect the registry to fill it in.
- Do not move node-specific logic into editor panels unless the node truly needs special UI.
