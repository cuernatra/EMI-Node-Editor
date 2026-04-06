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
- `deferredInputPins` (optional) for fields that should draw with inline input pins
- `renderStyle` for renderer/editor layout grouping

## Steps

1. Add the new `NodeType` value in `src/core/graph/types.h`.
2. Add a descriptor entry in the correct category file.
3. Write a named compile callback at the top of that same file.
4. Keep the callback readable by using helpers from `nodeCompileHelpers.h`.
5. Set `saveToken` explicitly.
6. Reuse an existing `renderStyle` and set `deferredInputPins` only when needed.
7. Build and run tests.
8. Verify `saveToken` round-trips through registry lookup.

## Descriptor checklist

When adding a node descriptor, confirm all of the following are set intentionally:

- `saveToken`: required and stable for serialization
- `renderStyle`: pick the closest existing style; add a new style only for truly new layout behavior
- `deferredInputPins`: include only input pin names that should render inline with their field row

If `deferredInputPins` contains a non-existent input pin name, registry validation fails at startup.

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

## When editor renderer edits are needed

Most node work should stay in `src/core/registry/nodes/*.cpp`.
Edit renderer files only if a node needs custom node-canvas field behavior.

- Add reusable field widgets in `src/editor/renderer/fieldWidgetRenderer.*`.
- Add node/link-aware custom field handling in `NodeRendererSpecialCases` inside `src/editor/renderer/nodeRenderer.*`.
- Handler signatures should use `FieldRenderContext& context`.
- Register new handlers in `HandleCustomFieldRendering`.

Before editing renderer files, try descriptor-first UI control:

- For inline pin+field rows, configure `deferredInputPins`.
- For existing layout families, reuse current `renderStyle` values.

## Custom dynamic layout behavior (editor graph pass)

If a node needs per-frame graph-aware layout/type repair that cannot be handled by
descriptor fields alone, use the graph refresh extension point:

1. Add a named helper in `src/editor/graph/graphEditorUtils.cpp`.
2. Add one entry to the ordered layout refresh table used by `RunAllLayoutRefreshes`.
3. Do not add new calls in `graphEditor.cpp`; it already runs the dispatcher.

This keeps custom layout behavior for already-registered node types as a one-file
editor change and preserves refresh order determinism.

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

## Pin or Field Input Logic (Unified Input Handling)

For any node input (such as DrawCell's X, Y, R, G, B or Set Variable's Default), the system uses a unified approach to determine the value:

- If the input pin is connected, the value comes from the connected node.
- If the pin is not connected, the value comes from the field (the value written in the node editor UI).

This is implemented using helper functions like `BuildNumberOperand` (see `nodeCompileHelpers.h`).

**Usage Example:**
```cpp
const Pin* xPin = FindInputPin(n, "X");
Node* x = xPin ? BuildNumberOperand(compiler, n, *xPin, "X") : MakeNumberLiteral(0.0);
```
Or, more simply, always use the helper:
```cpp
const Pin* xPin = FindInputPin(n, "X");
Node* x = BuildNumberOperand(compiler, n, *xPin, "X");
```

This ensures that all nodes can flexibly accept either a pin input or a written field value, with no need to duplicate logic for each node type.

**Best Practice:**
- Always use the provided helpers for number, boolean, or array inputs to get this behavior automatically.
- Do not reimplement this logic in each node's compile function.
