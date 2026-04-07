# Graph editor code

This folder owns the graph-editor runtime behavior:
- editing nodes and links
- save/load (serializer)
- per-frame refresh/repair utilities
- triggering graph compilation when the user presses Run


Start here:
- `graphEditor.cpp` — main graph editor behavior
- `graphSerializer.*` — save/load
- `graphCompilation.*` — calls the compiler and handles results
