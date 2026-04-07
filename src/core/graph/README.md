# Graph data model

This folder defines the *shape of the graph*: what a node is, what a pin is, and how links connect pins.

It is intentionally UI-free so it can be used by:
- the editor (to store and edit graphs)
- the compiler (to read graphs and compile them)

## Where to start

- `types.h` — the enums (`NodeType`, `PinType`, etc.)
- `visualNode.h` — what a node instance stores (pins + fields)

## When you edit this folder

- You are adding a new node/pin type.
- You are changing the graph data format.

If you are only adding a *new node behavior*, you usually want `core/registry/` instead.
