# Node Creation Guide

Shortest path for adding a node if you do not know the project yet.

## Usually edit these

1. `src/core/graph/types.h`
     Add the new `NodeType`.
2. `src/core/registry/nodes/*.cpp`
     Pick the right category file, add the compile function, then add the descriptor in `NodeRegistry::Register...Nodes()`.
3. `tests/*`
     Only if behavior, save/load, or compile output changed.

## Usually do not edit these

- `src/core/compiler/*`
    Only if existing compile helpers cannot express the node.
- `src/core/registry/nodeFactory.cpp`
    Usually no changes. It builds runtime `VisualNode`s from descriptors.
- `src/editor/renderer/*`
    Only if the node needs custom canvas UI beyond normal pins/fields.
- `src/editor/graph/*`
    Only if the node has dynamic layout or per-frame editor repair logic.

## Where everything is

- `src/core/graph/types.h`
    Global enums like `NodeType` and `PinType`.
- `src/core/registry/nodeDescriptor.h`
    Data model for a node template: pins, fields, compile callback, deserialize callback, category, palette variants, `saveToken`, `deferredInputPins`, `renderStyle`.
- `src/core/registry/nodeRegistry.cpp`
    Startup registration and validation, including deferred pin checks.
- `src/core/registry/nodeFactory.cpp`
    Turns a `NodeDescriptor` into a runtime `VisualNode` with pins and fields.
- `src/core/registry/nodes/eventNodes.cpp`
    Start and event-style nodes.
- `src/core/registry/nodes/dataNodes.cpp`
    Constants, variables, arrays, output.
- `src/core/registry/nodes/logicNodes.cpp`
    Math, compare, boolean logic.
- `src/core/registry/nodes/flowNodes.cpp`
    Delay, branch, loop, sequence, foreach, while.
- `src/core/registry/nodes/structNodes.cpp`
    Struct nodes.
- `src/core/registry/nodes/demoNodes.cpp`
    Demo/native-call style nodes.
- `src/core/registry/nodes/renderNodes.cpp`
    Runtime render-grid nodes.
- `src/core/registry/nodes/nodeCompileHelpers.h`
    Reusable compile helpers. Use this first before touching compiler internals.
- `src/core/graph/visualNode.h`
    Runtime node instance data stored in the graph.
- `src/editor/graph/graphSerializer.cpp`
    Save/load path. Uses `saveToken` and descriptor-driven node recreation.
- `src/editor/renderer/nodeRenderer.*`
    Special node-body drawing on the graph canvas.
- `src/editor/renderer/fieldWidgetRenderer.*`
    Reusable field widgets.
- `src/editor/graph/graphEditorUtils.cpp`
    Dynamic editor refresh/layout fixes.

## Normal node recipe

1. Add `NodeType` in `src/core/graph/types.h`.
2. Pick the correct category file under `src/core/registry/nodes/`.
3. Add a named `Compile...Node(...)` function in that file.
4. Add the descriptor in that same file, inside the matching `NodeRegistry::Register...Nodes()` function implemented there.
5. Set these fields intentionally:
     `label`, `pins`, `fields`, `compile`, `saveToken`, `category`, `renderStyle`.
6. Keep `deserialize = nullptr` unless the node has dynamic pins.
7. Use helpers from `nodeCompileHelpers.h` instead of writing compile plumbing again.
8. Build and test.

## Data structure responsibilities

- `PinDescriptor`
    Static pin definition for the node type.
- `FieldDescriptor`
    Static editable field definition.
- `NodeDescriptor`
    Full template used by editor, serializer, loader, and compiler.
- `VisualNode`
    Runtime node instance built from the descriptor by `nodeFactory.cpp`.

## Compilation responsibilities

- The compile callback lives next to the descriptor in `src/core/registry/nodes/*.cpp`.
- Most nodes should only use helpers from `nodeCompileHelpers.h`.
- For same-name pin/field pairs, the helpers now fall back automatically from disconnected input pin to field value.
- `GraphCompiler` should not gain a new node-specific method for routine nodes.
- If the node has unusual save/load pin layout, add a deserialize callback in the same file.

## UI responsibilities

- Default pins and fields come from the descriptor.
- `deferredInputPins` is the normal way to draw an input pin inline with its matching field.
- Deferred pin UI is descriptor-driven. You should not need to add node-specific renderer code just to make a deferred pin work.
- This works for all pin types. The field widget still follows the field's own `PinType` and options.
- Reuse an existing `renderStyle` if possible.
- Only edit `nodeRenderer.*` when the default descriptor-driven layout is not enough.
- The palette usually needs no manual edits. Registered descriptors are collected automatically in `src/editor/panels/leftPanel.cpp`.

## Deferred inputs

- Deferred input = an input pin that is not drawn in the normal input-pin list.
- Instead, it is drawn inline beside its matching field in the node body.
- Configure this with `NodeDescriptor::deferredInputPins` in `src/core/registry/nodeDescriptor.h`.
- For normal nodes, the names in `deferredInputPins` must match real input pin names in the descriptor and must also have same-name fields.
- For dynamic nodes, `deferredInputPins` can contain `"*"` to mean: any runtime input pin with a same-name field should render as deferred.
- Use this when a node should allow either a wire or a typed field value on the same row.
- If the input pin and field have the same name, the compile helpers can usually use the field automatically when the pin is disconnected.
- Serialization does not need special handling for deferred pins. Pins and fields still save and load through the normal graph serializer path.

## Deferred Pin Recipe

1. Add the input pin in the node descriptor.
2. Add a same-name field if you want pin-to-field fallback and inline deferred UI.
3. Add that pin name to `deferredInputPins`.
4. For dynamic runtime-generated pins, use `"*"` in `deferredInputPins` and make sure the runtime field names match the runtime pin names.
5. Use the normal helper in `nodeCompileHelpers.h` for the input type.

If those names match, you usually do not need renderer changes, compiler changes, or serializer changes.

## Edit / do not edit summary

- Edit: `types.h`, one registry node file, tests when needed.
- Sometimes edit: `nodeRenderer.*`, `fieldWidgetRenderer.*`, `graphEditorUtils.cpp`, `graphSerializer.cpp` for unusual save/load behavior.
- Rarely edit: `nodeDescriptor.h`, `nodeRegistry.cpp`, `nodeFactory.cpp`, `GraphCompiler`.

## Good defaults

- Keep compile logic beside the node descriptor.
- Keep `saveToken` explicit and stable.
- Prefer existing helpers and existing `renderStyle` values.
- Prefer same-name pin/field pairs for deferred inputs.
- If adding one node starts touching many unrelated files, stop and simplify the design.

Use `docs/NEW_NODE_CHECKLIST.md` as the final pre-merge checklist.
