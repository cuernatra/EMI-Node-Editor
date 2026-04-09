# Graph editor code

This folder owns the graph-editor runtime behavior:
- editing nodes and links
- save/load (serializer)
- per-frame refresh/repair utilities
- triggering graph compilation when the user presses Run


Start here:
- `graphEditor.cpp` — main graph editor behavior
- `graphSerializer.*` — save/load
- `graphExecutor.*` — drives the full run: build AST → compile bytecode → execute
