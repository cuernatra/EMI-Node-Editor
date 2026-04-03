# Architecture Overview

This project is split into a few clear layers.

## 1. App layer

Files:

- `src/app/app.cpp`
- `src/app/editorLayout.cpp`
- `src/main.cpp`

Responsibilities:

- Start the application.
- Create the window and frame loop.
- Place the main editor panels on screen.

## 2. Editor layer

Files:

- `src/editor/mainEditor.cpp`
- `src/editor/graph/*`
- `src/editor/panels/*`
- `src/editor/renderer/*`

Responsibilities:

- Handle user interaction.
- Draw the graph editor UI.
- Save and load graphs.
- Show the palette, inspector, preview, and console.

Rule:

- Editor code should not own node semantics unless it is unavoidable for presentation.

## 3. Core graph layer

Files:

- `src/core/graph/*`

Responsibilities:

- Store the graph model.
- Define nodes, pins, links, and runtime field values.
- Keep the data model separate from UI drawing.

## 4. Registry layer

Files:

- `src/core/registry/nodeDescriptor.h`
- `src/core/registry/nodeRegistry.cpp`
- `src/core/registry/nodeFactory.cpp`
- `src/core/registry/nodes/*.cpp`

Responsibilities:

- Define each node recipe and the full node structure.
- Map `NodeType` to descriptor data.
- Create runtime node instances.
- Provide compile and deserialize callbacks for nodes.

Think of `NodeDescriptor` as the schema for a node: if you want to know what the node is, what pins it has, what fields it exposes, how it compiles, and how it is saved, this is the file-level contract.

Important rule:

- Every node descriptor must define `saveToken` explicitly.
- The registry should not invent fallback serialization names.
- The registry tests verify token round-tripping and catch missing descriptor tokens early.

## 5. Compiler layer

Files:

- `src/core/compiler/graphCompiler.cpp`
- `src/core/compiler/pinResolver.cpp`

Responsibilities:

- Traverse the graph.
- Resolve pins and links.
- Build the EMI-Script AST.

Rule:

- The compiler should use node descriptors and helpers, not editor state.
- Add compiler features only when the existing helpers are not enough.

## 6. UI theme layer

Files:

- `src/ui/theme.h`

Responsibilities:

- Store colors, spacing, and shared visual constants.
- Keep style values out of the core logic.

## Node workflow

For a routine node addition:

1. Add the `NodeType`.
2. Add the node recipe in one category file under `src/core/registry/nodes/`.
3. Add a named compile callback in that same file.
4. Set `saveToken`.
5. Build and run tests.
6. Confirm the token resolves back to the same `NodeType` in registry tests.

For a dynamic-pin node:

1. Do the steps above.
2. Add a named deserialize callback in the same category file.
3. Confirm save/load preserves the dynamic pin layout.

## File ownership summary

- `core` owns the data and compile rules.
- `editor` owns interaction and presentation.
- `app` owns startup and frame composition.
- `ui` owns shared theme values.

## What to keep in mind

- The easiest code to maintain is code that stays near its responsibility.
- If a change forces edits across several layers, the design probably needs another boundary.
- Node-specific behavior should live with the node recipe unless it is truly a UI concern.
