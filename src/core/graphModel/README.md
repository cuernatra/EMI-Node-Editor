# core/graphModel/

The graph data model — what nodes, pins, and links store at runtime.
Both the editor and the compiler read from these types.

## Files

| File | What it contains |
|---|---|
| `types.h` | Enums: `NodeType`, `PinType`, `NodeRenderStyle` |
| `visualNode.h` | The `VisualNode` struct (id, title, pins, fields) |
| `pin.h / .cpp` | The `Pin` struct and `MakePin()` factory |
| `link.h / .cpp` | The `Link` struct (connects two pins) |
| `nodeField.h` | The `NodeField` struct (an editable value on a node) |
| `visualNodeUtils.h` | Free functions: `FindInputPin`, `FindOutputPin`, `FindField` |
| `idGen.h` | Generates unique ids for nodes and pins |

## When you edit this folder

- You are adding a new `NodeType` or `PinType` enum value.
- You are changing what data a node or pin stores.

## When you don't edit this folder

If you are adding a node behavior, compilation logic, or UI — you do not need to touch this folder.
Adding a new `NodeType` enum value here is usually the first step when adding a completely new node,
but the rest of the work happens in `registry/nodes/`.
