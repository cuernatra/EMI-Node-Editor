# Node files (where node authors work)

This folder contains the registered node types for the editor.

If you are adding a node and you are new to the compiler:

- You usually only edit **one** file in this folder (`dataNodes.cpp`, `flowNodes.cpp`, …).
- Add a `Compile...Node(GraphCompiler* compiler, const VisualNode& n)` function.
- Register it by setting `NodeDescriptor::compile` (or `compileFlow`, etc.).

## The “toolbox” (helpers)

Include this header from node files:

- `nodeCompileHelpers.h`

The helper header gives you:
- pin + field lookup helpers (find the pins/fields by name)
- “wire-or-field fallback” helpers (use a connected wire if present, otherwise parse the field)
- small AST builders (create literals and common AST shapes)

## Which callback should I use?

- `compile` — most nodes: emit one expression or one statement
- `compileFlow` — flow-control nodes: branch/loop/sequence that create sub-scopes
- `compilePrelude` — declarations that must exist before the main flow chain
- `compileTopLevel` — definitions that live next to `__graph__()` (e.g. user functions)
- `compileOutput` — different output pins return different values

If you are unsure, start with `compile`.

## Rules of thumb

- Prefer helper functions over adding new compiler features.
- Keep errors user-facing: `compiler->Error("Array Get node needs Array and Index inputs")`.
- Be explicit about ownership: if you create nodes and then early-return, delete what you created.
