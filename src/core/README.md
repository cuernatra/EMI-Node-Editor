# Core layer

This is the “engine” part of the editor. It contains the data model and the logic that is not UI.

## What lives here

- `graph/` — the graph data structures (nodes, pins, links)
- `registry/` — what each node type *is* (pins/fields + how it compiles)
- `compiler/` — turns a connected graph into an EMI-Script AST

## Where to start

- Adding or changing a node type: start in `registry/nodes/`
- Changing how compilation works in general: start in `compiler/graphCompiler.*`
- Adding a new node/pin type enum: start in `graph/types.h`

## Rules of thumb

- Keep UI out of `core/`.
- Keep node-specific behavior in the registry, not inside the compiler.
