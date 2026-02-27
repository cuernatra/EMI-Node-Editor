# Architecture & File Dependencies

## Core Data Structures

| File | Purpose | Key Types |
|------|---------|-----------|
| [pin.h](src/core/graph/pin.h) | Connection point on nodes | `Pin` (id, parentNodeId, type), `MakePin()` |
| [visualNode.h](src/core/graph/visualNode.h) | A node in the graph | `VisualNode` (id, pins, fields, position) |
| [link.h](src/core/graph/link.h) | Connection between pins | `Link` (startPinId, endPinId) |

## Node Type System

| File | Purpose | Key Types |
|------|---------|-----------|
| [nodeDescriptor.h](src/core/registry/nodeDescriptor.h) | Static template for node types | `NodeDescriptor`, `PinDescriptor`, `FieldDescriptor` |
| [nodeRegistry.h/cpp](src/core/registry/nodeRegistry.h) | Registry of all node types | `NodeRegistry` (singleton, holds descriptors) |
| [nodeFactory.h/cpp](src/core/registry/nodeFactory.h) | Creates nodes from descriptors | `CreateNodeFromType()`, `PopulateFromDescriptor()` |

## UI Components

| File | Purpose | Summary |
|------|---------|---------|
| [fieldWidget.h/cpp](src/core/registry/fieldWidget.h) | ImGui field editing | Number input, Boolean checkbox, String input, Operator dropdown |
| [graphEditor.h/cpp](src/editor/graphEditor.h) | Graph canvas & interaction | Node/link creation, deletion, dragging |
| [graphState.h/cpp](src/editor/graphState.h) | Graph state management | Stores nodes/links, ID generation, queries |
| [visualNode.cpp](src/core/graph/visualNode.cpp) | Pin rendering | `DrawPin()`, `DrawNode()` via imgui-node-editor |

## Compilation Pipeline

| File | Purpose | Summary |
|------|---------|-----------|
| [pinResolver.h/cpp](src/core/compiler/pinResolver.h) | Input → source mapping | `Resolve(ed::PinId)` returns PinSource (upstream node + index) |
| [graphCompiler.h/cpp](src/core/compiler/graphCompiler.h) | Visual nodes → AST | `BuildNode()` per descriptor, `BuildExpr()` recursive building |
| [graphCompilation.h/cpp](src/editor/graphCompilation.h) | High-level orchestration | `Compile()` validates graph, builds AST, TODO: invokes EMI parser |

## Serialization

| File | Purpose | Summary |
|------|---------|-----------|
| [graphSerializer.h/cpp](src/editor/graphSerializer.h) | Save/load graphs | `Save()` writes node/link IDs and field values; `Load()` restores |

---

## Dependency Graph

```
┌─────────────────────────────────────────────────────────────┐
│                    User Interactions (UI)                   │
└───────┬───────────────────────────────────────┬─────────────┘
         │                                       │
      DropBar                                  GraphState
   (palette drag)                         (stores nodes/links)
         │                                       │
         └──────────────┬────────────────────────┘
                           │
                     GraphEditor
                     (canvas, drag)
           │                                    │
           └────────────────┬───────────────────┘
                            │
                    VisualNode + Pin + Link
                    (data structures)  ◄─────────────────┐
                            │                            │
                            │                    NodeRegistry
                            │                    (type templates)
                            │                            │
                            └────────────────┬───────────┘
                                             │
                                     NodeFactory
                                     (creates instances)
                                             │
                        ┌────────────────────┘
                        │
                        ▼
                  FieldWidget
              (edits field values)
              (operator dropdowns)

────────────────────────────────────────────────────────────

         GRAPH → COMPILATION PIPELINE
         
                  GraphState
                  (nodes + links)
                        │
                        ▼
                  PinResolver
              (build input→source map)
                        │
                        ▼
                  GraphCompiler
          (visual nodes → AST via descriptors)
                   BuildNode() calls
               descriptor callbacks
                        │
                        ▼
                  GraphCompilation
            (orchestrates ▲ returns AST)
              (TODO: invoke EMI Parser::ParseAST)
```

---

## Data Flow: Adding a Node Type

1. **Define type**: Add enum to `NodeType` in [types.h](src/core/graph/types.h)
2. **Register**: Add `NodeDescriptor` in [nodeRegistry.cpp](src/core/registry/nodeRegistry.cpp)
3. **Implement builder**: Add method to [graphCompiler.cpp](src/core/compiler/graphCompiler.cpp)
4. **Auto-handled**: Factory, UI, serialization all work via descriptor

## Data Flow: Creating a Node (UI)

1. **Palette drag**: [dropBar.h](src/ui/dropBar.h) creates a drag payload with `NodeType`
2. **Spawn request**: [graphEditor.h](src/editor/graphEditor.h) receives payload and requests a new node
3. **Factory build**: [nodeFactory.h](src/core/registry/nodeFactory.h) builds pins/fields from the descriptor
4. **State update**: [graphState.h](src/editor/graphState.h) stores the new `VisualNode`

## Data Flow: Compiling

`GraphCompilation::Compile()` → `PinResolver::Build()` → `GraphCompiler::BuildNode()` → `BuildExpr()` (recursive) → AST