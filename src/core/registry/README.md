# Node registry

This folder is where node types are defined.

In plain language: this is the “catalog” of nodes the editor knows about.
Each node definition includes:
- pins and fields (what the user sees)
- save/load identity (stable `saveToken`)
- compilation callbacks (how it becomes script AST)

Start here:
- `nodeDescriptor.h` — the “schema” for a node type
- `nodeRegistry.*` — registration + validation
- `nodeFactory.*` — creates runtime `VisualNode` instances from descriptors
- `nodes/` — the actual node type implementations

## Where you usually edit

- Adding a new node: edit exactly one file in `nodes/` and register the descriptor there.
- Changing how nodes are created from descriptors: `nodeFactory.*`.
- Changing validation rules: `nodeRegistry.*`.

If you're adding a node, you will spend most of your time in `nodes/`.
