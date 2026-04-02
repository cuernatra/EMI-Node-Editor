
# AO_Emi file guide (developer)

## Principles

- Keep logic and UI separate.
- `src/core` should not contain ImGui UI rendering.
- Theme values (colors/fonts) live in `src/ui` (do not hardcode UI colors in random files).

## High-level architecture

- `src/core/`
  - Owns the graph model and compilation logic.
  - Main parts:
    - `core/graph/`: graph data types (nodes, pins, links, ids)
    - `core/registry/`: node type definitions (descriptors) + node factory
    - `core/compiler/`: graph → EMI-Script AST compilation

- `src/editor/`
  - Owns editor state + all UI rendering.
  - Main parts:
    - `editor/graph/`: editor-side graph state, serialization, and compile bridge
    - `editor/panels/`: ImGui panels (left palette, top bar, console, inspector, preview)
    - `editor/renderer/`: ImGui + node-editor rendering (nodes, pins, links, field widgets)
    - `editor/widgets/`: small shared UI widgets (example: node preview)

- `src/app/`
  - Application shell: SFML window loop + top-level layout.
  - `app/app.*`: main loop, ImGui-SFML init, per-frame draw
  - `app/editorLayout.*`: layout compositor that places panels + main editor

- `src/ui/`
  - Theme tokens only (colors, shared fonts). No panels/widgets.

## How to add a new node (short checklist)

1) Must: add a new `NodeType`
   - File: `src/core/graph/types.h`

2) Must: register the node type (this makes it appear in the editor)
   - File: `src/core/registry/nodeRegistry.cpp`
   - Add a `NodeDescriptor` entry:
     - `label` (title shown in the node header)
     - `pins` (name/type/input-output)
     - `fields` (editable values stored as strings)
     - compile callback (usually `compiler->BuildYourNode(n)`)

3) Optional: special-case node creation
   - Usually nothing to do: `src/core/registry/nodeFactory.*` creates pins/fields from the descriptor.
   - Only needed for dynamic pin layouts (variable pin count, schema-driven pins, etc.).

4) Must (to compile): implement compilation
   - Files:
     - `src/core/compiler/graphCompiler.h` (declare `BuildYourNode`)
     - `src/core/compiler/graphCompiler.cpp` (implement `BuildYourNode`)

5) Optional: custom UI behavior
   - File: `src/editor/renderer/nodeRenderer.cpp`
   - Most nodes “just work” because pins/fields come from the descriptor.
   - Add code here only if you need a custom widget/behavior.

Notes:
- The left palette auto-enumerates all registered node descriptors.
  You do not manually add nodes to the palette.
  Unknown/unclassified nodes end up under the “More” section.

## Compiler + AST overview (conceptual)

### What “compile” means here

- You draw a graph of nodes.
- The compiler turns that graph into something the EMI runtime can execute.

### What is an AST?

- AST = “Abstract Syntax Tree”.
- Think of it as a structured version of code.
  Example: instead of the text `1 + 2`, the AST is a tree: `Add(Number(1), Number(2))`.
- In this project we build the AST directly (we do not generate source code text).

### Pipeline (end-to-end)

1) Node types define how they compile
   - `src/core/registry/nodeDescriptor.h`
   - `src/core/registry/nodeRegistry.cpp`
   Each `NodeType` has a `NodeDescriptor`, including a compile callback like `compiler->BuildOperator(node)`.

2) `GraphCompiler` builds an AST from the visual graph
   - `src/core/compiler/graphCompiler.*`
   - `src/core/compiler/pinResolver.*`
   `PinResolver` answers: “for this input pin, what output is it connected to?”.
   `GraphCompiler` uses that to recursively build expressions and flow statements.

3) `GraphCompilation` runs it in the VM
   - `src/editor/graph/graphCompilation.*`
   It calls `GraphCompiler::Compile(nodes, links)`, feeds the AST into the VM, then runs `__graph__();`.

Notes:
- The compiled graph is wrapped into a function named `__graph__` (`GraphCompiler::kGraphFunctionName`).
- If something fails, `GraphCompiler` sets `HasError` + a message, and the editor shows it in the console.

