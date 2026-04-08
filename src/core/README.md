# core/

The engine layer — data model and logic with no UI code.

## Folders

| Folder | What it does |
|---|---|
| `graph/` | Data structures: what a node, pin, and link store |
| `registry/` | Node catalog: what each node type looks like and how it compiles |
| `compiler/` | Turns a connected graph into an AST the VM can run |

## Where to go for common tasks

| Task | Where |
|---|---|
| Add or edit a node | `registry/nodes/` |
| Change what a node looks like (pins, fields) | `registry/nodes/` |
| Add a new NodeType or PinType enum | `graph/types.h` |
| Change how the compiler works | `compiler/graphCompiler.*` |
| Change how nodes are created at runtime | `registry/nodeFactory.*` |

## Rule

Keep UI out of `core/`. Keep node behavior in the registry, not in the compiler.
