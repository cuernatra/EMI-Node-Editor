
# AO_Emi file guide (developer)

## Principles

- Keep the code that does work separate from the code that draws the screen.
- `src/core` should not draw ImGui UI.
- Colors and fonts live in `src/ui`. Do not hardcode UI colors in random files.

## High-level architecture

- `src/core/`
  - Holds the graph data and the code that turns graphs into executable form.
  - Main parts:
    - `core/graph/`: graph data like nodes, pins, links, and ids
    - `core/registry/`: node definitions and node creation helpers
    - `core/compiler/`: turns a graph into EMI-Script AST code

Core file ownership boundaries:
- `core/graph/types.h`: shared node and pin type lists.
- `core/graph/nodeField.h`: runtime per-node editable field values.
- `core/graph/visualNode.h`: runtime state for one node on the graph.
- `core/registry/nodeDescriptor.h`: the node structure definition and contract. It says what the node is, which pins and fields it has, how it compiles, how it deserializes, and which save token it uses.
- `core/registry/nodeRegistry.*`: looks up node recipes by type or token.
- `core/registry/nodeFactory.*`: builds live `VisualNode` objects from recipes.

- `src/editor/`
  - Holds editor state and all screen drawing.
  - Main parts:
    - `editor/graph/`: editor graph state, save/load, and compile bridge
    - `editor/panels/`: the side panels, top bar, console, inspector, and preview
    - `editor/renderer/`: drawing for nodes, pins, links, and field controls

Graph refresh pipeline ownership:
- `editor/graph/graphEditor.cpp` runs the fixed three-pass refresh order each frame.
- `editor/graph/graphEditorUtils.cpp` owns the ordered per-node layout refresh dispatcher (`RunAllLayoutRefreshes`).
- For custom dynamic layout behavior on an already-registered node type, add a helper and one table entry in `graphEditorUtils.cpp`.

Renderer ownership split:
- `src/editor/renderer/fieldWidgetRenderer.*`
  - Owns generic field widgets and read-only field display helpers.
  - Should not depend on graph-link context to decide behavior.
- `src/editor/renderer/nodeRenderer.*`
  - Owns node layout, pin placement, and link-aware field behavior.
  - Calls fieldWidgetRenderer helpers for reusable field UI.
  - Custom field special-cases live in `NodeRendererSpecialCases` and use `FieldRenderContext& context`.
  - New custom handlers must be chained through `HandleCustomFieldRendering`.

- `src/app/`
  - Application shell: SFML window loop and top-level layout.
  - `app/app.*`: main loop, ImGui-SFML setup, and frame drawing
  - `app/editorLayout.*`: places the panels and the main editor area

- `src/ui/`
  - Theme values only, like colors and shared fonts. No panels or widgets.

## Node authoring policy

For most node changes, the main work happens in `src/core/registry/nodes/`.

- Add or remove `NodeType` values in `src/core/graph/types.h`.
- Add or remove node recipes in the category files under `src/core/registry/nodes/`.
- Put named compile and deserialize callbacks at the top of the same category file, above the `Register` calls.
- Reuse the shared helpers in `src/core/registry/nodes/nodeCompileHelpers.h`.
- Do not add new node-specific `GraphCompiler::BuildX` methods.

`src/core/registry/nodeRegistry.*` is shared support code, not a normal per-node edit target.

`src/core/compiler/graphCompiler.*` should only change when the compiler itself needs a new ability, not for routine node add/remove work.

## Plain language node guide

- For a normal node change, you usually edit two files.
- The usual files are `src/core/graph/types.h` and one file in `src/core/registry/nodes/`.
- For custom dynamic layout behavior on an existing node type, you usually edit one editor file: `src/editor/graph/graphEditorUtils.cpp`.
- Every node recipe must set `saveToken`. It is the stable save/load name for the node.
- The registry fails fast if `saveToken` is missing; there is no fallback token generation path.
- To add a node, give it a type, add it to the right category file, write the named compile callback there, then build and test.
- To remove a node, delete its recipe and remove its type.
- Keep the inspector drop-downs simple. Do not use the node-canvas popup helper there, because it can crash outside the canvas.
- For array values in the inspector, use the normal inspector controls or plain text editing. Do not use the node-editor popup helper there.
- If a node looks wrong after a change, rebuild the app and run the tests before changing more code.

## Where to put a node

- `src/core/registry/nodes/eventNodes.cpp`: start and event-style nodes
- `src/core/registry/nodes/dataNodes.cpp`: constants, variables, arrays, and output
- `src/core/registry/nodes/logicNodes.cpp`: math, comparison, and boolean logic
- `src/core/registry/nodes/flowNodes.cpp`: delay, sequence, branch, loop, foreach, and while
- `src/core/registry/nodes/structNodes.cpp`: struct define and struct create

## Create node: simple path

Use this when node compile logic is a direct expression/call/indexer using existing helpers.

1) Add enum
- File: `src/core/graph/types.h`
- Add a new `NodeType` value.

2) Decide category file
- Event-like: `src/core/registry/nodes/eventNodes.cpp`
- Data/value: `src/core/registry/nodes/dataNodes.cpp`
- Logic/comparison: `src/core/registry/nodes/logicNodes.cpp`
- Flow/control: `src/core/registry/nodes/flowNodes.cpp`
- Struct/schema: `src/core/registry/nodes/structNodes.cpp`

3) Register descriptor in that file
- File: one of `src/core/registry/nodes/*.cpp`
- Add `Register({ ... })` entry with:
  - `type`
  - `label`
  - `pins`
  - `fields`
  - named compile callback
  - named deserialize callback (`nullptr` unless needed)
  - `category`
  - optional `paletteVariants`
  - `saveToken`
  - optional `deferredInputPins`
  - `renderStyle`

4) Write named compile callback in the same file
- Prefer helpers from `nodeCompileHelpers.h`:
  - `FindInputPin`, `FindField`
  - `BuildNumberInput`, `BuildArrayInput`, `BuildOutputValue`, etc.
  - `MakeNumberLiteral`, `MakeBoolLiteral`, `MakeStringLiteral`
- Use the compiler's built-in helpers instead of new node-specific compiler methods:
  - `EmitBinaryOp`
  - `EmitUnaryOp`
  - `EmitFunctionCall`
  - `EmitIndexer`
  - `BuildExpr`

5) Configure node UI in descriptor fields (automated)
- `FieldDescriptor` can include a list of choices for drop-downs.
- If you set `options`, the node body and inspector show a combo box automatically.
- If a field has the same name as an input pin, the inspector handles read-only behavior automatically when that pin is connected.
- For node-canvas inline pin+field behavior, set `deferredInputPins` instead of adding renderer branches.
- Reuse an existing `renderStyle` whenever possible so routine nodes do not require renderer edits.
- This means most node UI behavior lives in `src/core/registry/nodes/*.cpp` without changing editor renderer files.

6) Set a stable save token
- File: same descriptor block
- Set `saveToken` explicitly for every node.
- Do not rely on a fallback token. Missing `saveToken` should fail fast.

7) Validate
- Build and run tests.
- The tests now check that all registered node types have compile callbacks and that save tokens round-trip through the registry.
- Check that the node appears in the left palette and can be dragged into the graph.
- Save the graph, reload it, and confirm the node comes back correctly.

Files you usually should NOT edit for normal node creation:
- `src/core/compiler/graphCompiler.*`
- `src/core/registry/nodeFactory.*`
- `src/editor/renderer/nodeRenderer.cpp`
- `src/editor/panels/leftPanel.cpp`

Simple compile callback pattern:

```cpp
[](GraphCompiler* compiler, const VisualNode& n) -> Node*
{
    const Pin* a = FindInputPin(n, "A");
    const Pin* b = FindInputPin(n, "B");
    if (!a || !b)
    {
        compiler->Error("MyNode needs A and B");
        return nullptr;
    }

    Node* lhs = BuildNumberInput(compiler, n, *a, "A");
    Node* rhs = BuildNumberInput(compiler, n, *b, "B");
    return compiler->EmitBinaryOp(Token::Add, lhs, rhs);
}
```

## Create node: complex path

Use this when a node has changing pins, more than one palette entry, or compile output that needs several steps.

### A) Dynamic pin layout (deserialize callback)

Add a `deserialize` callback if the node needs to rebuild its pins from saved pin ids.

- Good examples: Variable Get/Set variants, Sequence variable outputs, and Struct Create fields that come from a schema.
- Keep `PopulateExactPinsAndFields` in the category file for the exact-match fallback path.

Pattern:

```cpp
[](VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds) -> bool {
    if (pinIds.size() == desc.pins.size())
        return PopulateExactPinsAndFields(n, desc, pinIds);

    // custom pin reconstruction...
    return true;
}
```

### B) Multi-statement compile output

Return a `Token::Scope` and add the statement nodes to `children`.

- Use this for nodes that need temporary variables and more than one compile step.
- Keep ownership clear: if one step fails, delete the partial work and return `nullptr`.

### C) Palette variants

If one `NodeType` should appear more than once in the palette, use `paletteVariants` in the descriptor.

```cpp
{
  { "Set Variable", "Variable:Set" },
  { "Get Variable", "Variable:Get" }
}
```

## Remove node: simple path

1) Remove the node recipe from its category file under `src/core/registry/nodes/`.
2) Remove `NodeType` enum value from `src/core/graph/types.h`.
3) Remove any tests, sample graphs, or fixtures that still use the node.
4) Build and run tests.

## Remove node: complex path

Also check and clean:

- Custom pin rebuild code in the same category file.
- Extra palette entries and any payload assumptions.
- References in `graphSerializer` fixtures or sample graphs.
- Any helper in `nodeCompileHelpers.h` that is no longer used.
- Inspector or preview handling, if that node had special UI code.

## Decision rule: when GraphCompiler can stay untouched

You do not need to change `graphCompiler` when your new or changed node can be built with:

- pin and field reads from helpers,
- `BuildExpr` for connected inputs,
- the existing emitters (`EmitBinaryOp`, `EmitUnaryOp`, `EmitFunctionCall`, `EmitIndexer`),
- and optional `Token::Scope` composition.

You only need to change `graphCompiler` if you add a new compiler ability, such as:

- a new AST emitter,
- flow behavior that the current builders cannot handle,
- or a change to how the compiler resolves or walks the graph.

## Quick QA checklist for node CRUD

- Node appears in palette under correct category.
- Spawn, save, reload works (token and deserialize path valid).
- Compile succeeds with node connected and disconnected defaults.
- Error message quality is user-facing and clear.
- No new node-specific methods were added to `graphCompiler.h/.cpp`.

Recommended local commands before PR:
- `cmake --build build --config Debug --target emi-editor`
- `cmake --build build --config Debug --target tests`

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
  - `src/core/registry/nodes/*.cpp`
  Each `NodeType` has a `NodeDescriptor`, including a named compile callback that uses compiler primitives and shared helpers.

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

