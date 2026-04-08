# core/registry/

The node catalog — what each node type looks like and how it compiles.

## Files

| File | What it does |
|---|---|
| `nodeDescriptor.h` | The `NodeDescriptor` struct: pins, fields, compile callbacks |
| `nodeRegistry.*` | Stores all descriptors; `Find()` looks up a node type at compile time |
| `nodeFactory.*` | Creates `VisualNode` instances from descriptors (used when spawning nodes) |
| `nodes/` | The actual node implementations — **this is where you work** |

## When you edit what

- **Adding or changing a node:** edit a file in `nodes/`. You rarely need to touch anything else here.
- **Changing how nodes are spawned or initialized:** `nodeFactory.*`.
- **Changing validation or registration rules:** `nodeRegistry.*`.

## What a NodeDescriptor is

A `NodeDescriptor` is a static description of one node type: its display name,
its pins and fields, and the callbacks the compiler calls when it encounters that node.
See `nodeDescriptor.h` for the full list of fields and an explanation of each callback.
