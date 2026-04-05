# Plan: Migrate C++ AStarPreview to Compiled Visual Node Graph

## Goal

The C++ `AStarPreview` node runs A* in `renderAStarNodes()` and renders directly
to SFML. The goal is to replace it with a visual node graph that compiles to
EMI-Script AST, runs in the EMI VM, and renders through the `drawcell`/`cleargrid`
native bridge. The end result should behave identically to the current animation:
20×20 grid, agent (red) follows A* path at 50 ms/step, random goals (green), path
drawn ahead in blue.

The reference implementation is `game.ril` (EMI-Script demos) and
`renderAStarNodes()` in `graphPreviewPanel.cpp`.

---

## What Is Already Done (Phase 1)

The rendering bridge is complete:
- Native functions `drawcell(x,y,r,g,b)`, `cleargrid(w,h)`, `rendergrid()` registered
  via `EMI_REGISTER` in `previewNatives.cpp`
- `GraphPreviewPanel` owns a mutex-guarded pixel buffer; `renderPixelBuffer()` draws
  it to the SFML window each frame
- Three node types exist: `DrawCell`, `ClearGrid`, `RenderFrame`

The current `graph.txt` is a static proof-of-concept (draws fixed cells once, no loop).
It does not animate. This plan covers the remaining work.

Incremental migration status:
- Phase 2a started: `Sqrt` and `Random Int` nodes are implemented and compile to
  `Math.Sqrt(a)` and `random(min,max)`.
- `random(lo, hi)` native is registered in `previewNatives.cpp`.
- Legacy `AStarPreview` path remains intact as fallback while editor-authored
  graph logic is migrated in small steps.
- First editor-authored behavior milestone added: graph-driven loop now handles
  random goal selection and per-step agent movement (1D), rendered with
  `DrawCell` + `RenderFrame` + `Delay` in the node graph.

---

## Missing Node Types (all phases combined)

Before writing a single animated graph, these node types must exist. The table
below shows each gap, what it compiles to, and which phase adds it.

| Phase | Node | Save Token | Compiles to EMI-Script |
|---|---|---|---|
| 2 | Sqrt | `Sqrt` | `Math.Sqrt(a)` |
| 2 | Random Int | `RandomInt` | `random(min, max)` + register native |
| 2 | Array Push | `ArrayPush` | `Array.Push(array, value)` |
| 2 | Array Push Front | `ArrayPushFront` | `Array.PushFront(array, value)` |
| 2 | Array Push Unique | `ArrayPushUnique` | `Array.PushUnique(array, value)` |
| 2 | Array Clear | `ArrayClear` | `Array.Clear(array)` |
| 2 | Array Resize | `ArrayResize` | `Array.Resize(array, size)` |
| 3 | Struct Get Field | `StructGetField` | `obj.field` (data node, no flow) |
| 3 | Struct Set Field | `StructSetField` | `obj.field = value` (flow node) |
| 3 | Copy | `Copy` | `Copy(value)` |

All other required logic (Operator, Comparison, Logic, Branch, While, ForEach,
Variable, Delay, Function, StructCreate) already exists.

---

## Phase 2 — Utility Nodes

**Deliverable**: New node types for the seven missing operations + `random` native.

### 2a — Math nodes

**Sqrt**  
Pins: `A` (Number in), `Result` (Number out)  
Fields: `A` default = 0.0  
Compiles to: `Math.Sqrt(a)` (use `EmitFunctionCall("Math.Sqrt", {aExpr})`)  
File: `logicNodes.cpp` + `types.h`

**Random Int**  
Pins: `Min` (Number in), `Max` (Number in), `Result` (Number out)  
Fields: `Min` = 0, `Max` = 10  
Compiles to: `random(min, max)`  
Also register `random` in `previewNatives.cpp`:
```cpp
static void random_impl(double lo, double hi)  // returns double through EMI_REGISTER
```
File: `previewNatives.cpp`, `logicNodes.cpp`, `types.h`

### 2b — Array flow nodes (all take `In`/`Out` flow pins + array input)

**Array Push** — `Array.Push(array, value)` — one value input  
**Array Push Front** — `Array.PushFront(array, value)` — one value input  
**Array Push Unique** — `Array.PushUnique(array, value)` — one value input  
**Array Clear** — `Array.Clear(array)` — no value input  
**Array Resize** — `Array.Resize(array, size)` — one number input  

Each compiles to the corresponding `EmitFunctionCall`. Arrays are identified by a
Variable Get pin on the `Array` input pin, same pattern as existing ArrayAddAt.  
File: `dataNodes.cpp`, `types.h`

---

## Phase 3 — Struct Access Nodes

The A* algorithm reads and writes struct fields (Coord.x, AStarNode.prevNode, etc.).
Existing `StructCreate` makes instances; nothing reads or writes individual fields.

### 3a — Struct Get Field (data node)

Pins: `Object` (Any in), `Value` (Any out)  
Fields: `Field Name` (String, default = "x")  
Compiles to: identifier path `obj.field` built from the connected object expression
and the `Field Name` field literal.  
Implementation: resolve `Object` input pin → `BuildExpr`, then wrap in a field-access
AST node (`Token::FieldAccess` or equivalent member accessor).  
File: `structNodes.cpp`, `types.h`

### 3b — Struct Set Field (flow node)

Pins: `In` (Flow in), `Object` (Any in), `Value` (Any in), `Out` (Flow out)  
Fields: `Field Name` (String, default = "x")  
Compiles to: `obj.field = value` as an assignment statement.  
File: `structNodes.cpp`, `types.h`

### 3c — Copy (data node)

Pins: `Value` (Any in), `Result` (Any out)  
Compiles to: `Copy(value)`  
File: `logicNodes.cpp`, `types.h`

---

## Phase 4 — Object Definitions as Graph Preamble

In `game.ril`, `Coord` and `Node` (AStarNode) are declared as `object` types.
In the visual graph, these live as `StructDefine` nodes at the start, connected
by the flow chain before any algorithm nodes.

```
[Start]
  → [StructDefine: "Coord"   fields=["x:Number","y:Number"]]
  → [StructDefine: "AStarNode" fields=["actionID:Number","prevNode:Any","state:Any","Cost:Number","G:Number"]]
  → [Variable Set: Actions = [Coord{-1,0}, Coord{1,0}, Coord{0,-1}, Coord{0,1}]]
  → ... (game loop below)
```

The `Actions` array of direction Coord objects must be initialized once. This
requires four `StructCreate` nodes creating Coord values and one array literal
or sequential ArrayPush calls to build the Actions array.

No new node types needed for this phase — only `StructDefine`, `StructCreate`,
`Variable Set`, `ArrayPush`.

---

## Phase 5 — A* Helper Functions as Function Node Graphs

Each helper function from `game.ril` becomes a `Function` define sub-graph.
They are authored in the editor as named Function nodes with their own internal
flow, compiled to EMI-Script function definitions.

Port in dependency order:

### 5a — `equal(l, r) → bool`
Two `Struct Get Field` nodes (l.x, r.x), one `Comparison ==`, one for (l.y, r.y),
one `Comparison ==`, one `Logic AND`. Returns the boolean.

### 5b — `GetCost(state, g) → Number`
`Struct Get Field` for Target.x, Target.y, state.x, state.y.  
`Operator -` for x_dist and y_dist.  
`Operator *` for x_dist*x_dist and y_dist*y_dist.  
`Operator +` to sum squares.  
`Sqrt` node.  
`Operator *` by 2.  
`Operator +` with g. Returns result.

### 5c — `ValidPreCheck(loc, act) → bool`
`Struct Get Field` for loc.x, loc.y, act.x, act.y.  
`Operator +` for candidate x and y.  
Four `Comparison` nodes for (x >= 0), (x < W), (y >= 0), (y < H).  
Three `Logic AND` to combine. Returns bool.  
(No wall check needed for the simple grid — the C++ demo has no walls.)

### 5d — `Act(state, action) → Coord`
`Struct Get Field` for state.x, state.y, action.x, action.y.  
Two `Operator +` nodes.  
`StructCreate` for Coord with the two sums. Returns the new Coord.

### 5e — `transition(current, act, actionID) → AStarNode`
Calls `Act(current.state, act)` via a Function Call node.  
`StructCreate` for AStarNode with: actionID, current (as prevNode), newState Coord,
Cost=0, G=0. Returns the new node.

### 5f — `calculateCost(n : AStarNode)`
`Struct Set Field` n.G = 0.  
`Variable Set` current = n.  
`While` (current != null):  
  - `Struct Get Field` current.state, current.actionID.  
  - `Operator +` n.G += GetStateCost (= 1.0, always).  
  - `Struct Set Field` current = current.prevNode.  
`Function Call` GetCost(n.state, n.G) → result.  
`Struct Set Field` n.Cost = result.

### 5g — `getNextLegalStates(n : AStarNode) → Array`
`Variable Set` nextStates = [].  
`ForEach` Actions array, element = act:  
  - `Function Call` ValidPreCheck(n.state, act) → bool.  
  - `Branch` on bool:  
    - True: `Function Call` transition(n, act, index) → newState.  
    - `ArrayPush` nextStates with newState.  
`Variable Get` nextStates. Return it.

### 5h — `findPath(begin : Coord, target : Coord) → Array`

This is the main A* loop. Use the same approach as game.ril:

```
Variable Set openList_keys   = []
Variable Set openList_values = []
Variable Set closedList      = []
Variable Set currentNode     = AStarNode{0, null, begin, 0, 0}
ArrayPushUnique closedList ← currentNode.state

While (Visit(currentNode.state)):   [Visit = !equal(Target, state)]
  ForEach getNextLegalStates(currentNode):
    Branch child == null → skip
    Call calculateCost(child)
    Branch find(closedList, child.state, equal) != null → skip
    [check openList for child.state, update if better cost]
  [find min-cost node in openList_values → currentNode]
  ArrayPushUnique closedList ← currentNode.state
  [Erase currentNode from openList]

Variable Set actionPath = []
While currentNode.prevNode != null:
  ArrayPushFront actionPath ← currentNode.actionID
  Struct Set Field currentNode = currentNode.prevNode
Return actionPath
```

The open-list operations (Map.insert / Map.Get / Map.Erase) can be expressed using
pairs of parallel arrays (keys + values) with ForEach scans. This requires Loop +
Comparison + ArrayRemoveAt + ArrayReplaceAt + ArrayAddAt nodes — all existing.

A `find` helper sub-graph (ForEach + Branch + Variable Set + break-on-found) is also
needed here.

---

## Phase 6 — Game Loop as Main Graph

This replaces the `[Start] → ClearGrid → DrawCell... → RenderFrame` static graph.

The full loop, mapping directly to `game()` from `game.ril`:

```
[Start]
  → [ClearGrid: W=20, H=20]
  → [StructDef: Coord] → [StructDef: AStarNode]
  → [Variable Set: Actions = [...4 Coord directions...]]
  → [Variable Set: charCoord = Coord{0,0}]
  → [Variable Set: actions = []]
  → [Variable Set: actionIdx = 0]
  → [Variable Set: Target = Coord{5,5}]

  → [While: condition = true]  ← infinite game loop
      Body:
        → [Variable Get: actionIdx] [ArrayLength: actions]
        → [Comparison: actionIdx < size] → bool

        → [Branch bool]
            True:  (agent is moving along path)
              → [DrawCell: charCoord.x, charCoord.y, 40, 40, 40]    ← erase
              → [ArrayGetAt: actions[actionIdx] → actionID]
              → [ArrayGetAt: Actions[actionID] → act]
              → [Function Call: ValidPreCheck(charCoord, act)]
              → [Branch valid]
                  True:
                    → [Struct Set Field: charCoord.x += act.x]
                    → [Struct Set Field: charCoord.y += act.y]
              → [DrawCell: charCoord.x, charCoord.y, 220, 50, 50]   ← agent red
              → [Operator +: actionIdx + 1 → Variable Set actionIdx]

            False: (path exhausted, pick new goal)
              → [RandomInt: 0, 19 → Variable Set Target.x]
              → [RandomInt: 0, 19 → Variable Set Target.y]
              → [Function Call: findPath(charCoord, Target) → actions]
              → [Variable Set: actionIdx = 0]
              → [Variable Set: previewLoc = Copy(charCoord)]
              → [ForEach: actions]
                  Body:
                    → [ArrayGetAt: Actions[actionID] → stepAct]
                    → [Struct Set Field: previewLoc.x += stepAct.x]
                    → [Struct Set Field: previewLoc.y += stepAct.y]
                    → [DrawCell: previewLoc.x, previewLoc.y, 0, 100, 220]  ← path blue
              → [DrawCell: Target.x, Target.y, 0, 180, 60]           ← goal green

        → [RenderFrame]
        → [Delay: 50]
```

After this phase the graph compiles, the VM runs it on the background thread,
and the SFML window shows the same animation as the C++ AStarPreview.

### Phase 6.1 — Minimum Viable Animation (do this first)

Before wiring full A*, get motion working end-to-end with a tiny loop:

```
[Start]
  → [ClearGrid: W=20, H=20]
  → [Variable Set: x = 0]
  → [Variable Set: y = 0]
  → [Variable Set: dx = 1]

  → [While: true]
      Body:
        → [DrawCell: x, y, 30, 30, 30]        ← erase previous
        → [Operator +: x + dx → x]
        → [Branch: (x >= 19) OR (x <= 0)]
            True: [Operator *: dx * -1 → dx]  ← bounce left/right
        → [DrawCell: x, y, 220, 50, 50]       ← draw agent
        → [RenderFrame]
        → [Delay: 50]
```

If this animates smoothly, the render bridge, loop control, and delay cadence are
validated. Then layer Target + findPath on top.

---

## Phase 7 — Remove C++ AStarPreview

Once the node graph produces identical behavior:
- Delete `RunAStarGrid`, `AStarAnimState`, `renderAStarNodes` from
  `graphPreviewPanel.cpp` (~200 lines)
- Remove `AStarPreview` from `types.h` and its descriptor from `eventNodes.cpp`
- Add a save-token alias in `graphSerializer.cpp` so any saved `.graph` with
  `AStarPreview` nodes degrades gracefully (renders nothing, no crash)

---

## Order of Work

```
Phase 1  ✓  DONE — DrawCell / ClearGrid / RenderFrame nodes + pixel buffer
Phase 2     Add 7 utility node types (Sqrt, RandomInt, 5 array operations)
            Register random() native in previewNatives.cpp
Phase 3     Add StructGetField, StructSetField, Copy node types
Phase 4     Author struct definitions in graph (Coord, AStarNode, Actions init)
Phase 5     Author helper function sub-graphs (equal → findPath)
            Test each in isolation before wiring to game loop
Phase 6     Author full game loop graph, verify animation matches C++ demo
Phase 7     Delete C++ AStarPreview code
```

Phases 2 and 3 are code changes (new node type registrations).  
Phases 4–6 are editor work (authoring node graphs, no new C++ needed).  
Phase 7 is cleanup.

---

## Technical Notes

**EMI-Script intrinsics available**: `Math.Sqrt`, `Array.Size`, `Array.Push`,
`Array.PushFront`, `Array.PushUnique`, `Array.Resize`, `Array.Clear`,
`Array.RemoveIndex`, `Array.Insert`, `Copy`, `delay`, `println`.  
All can be called via `EmitFunctionCall` in compile callbacks.

**`random(lo, hi)` native**: Register in `previewNatives.cpp` using
`std::uniform_int_distribution` with a static `std::mt19937` seeded at startup.
The EMI `EMI_REGISTER` macro deduces parameter types from the `std::function`.
Use inclusive bounds intentionally: for a 20x20 grid call `random(0, 19)`.

**StructGetField AST**: Check how the compiler represents member access
(`obj.field`). In EMI-Script this is a dotted identifier chain. The
`GraphCompiler` may need a helper (`EmitFieldAccess(objExpr, fieldName)`) or
the existing `BuildExpr` infrastructure handles it via `Token::FieldAccess`.
Confirm by reading `graphCompiler.cpp` before implementing.

**findPath open list**: The game.ril `MapContainer` (key=Coord, value=AStarNode)
is emulated by two parallel arrays in the node graph. All MapContainer operations
decompose into `ForEach` scans + `ArrayAddAt`/`ArrayReplaceAt`/`ArrayRemoveAt`.
This is verbose but requires no new node types.

**Thread safety**: The VM runs `__graph__()` on a background thread. `drawcell`
and `cleargrid` already lock `m_pixelBuffer.mutex`. No additional synchronization
needed.

**delay(50)**: The `delay` intrinsic sleeps in 10 ms chunks and checks
`IsRuntimeInterruptRequested()`, so the graph stays interruptible (stop button works).

---

# Blueprint-Inspired Detailed Implementation Plan

This section provides a detailed, Blueprint-inspired breakdown for each phase, focusing on node design, pin conventions, and graph authoring best practices. Use this as a reference for both C++ implementation and visual graph construction.

## Node Design Principles (Blueprint Parallels)
- **Every node is a typed operation**: Each node corresponds to a single, composable operation (math, logic, struct access, flow control, etc.), similar to Unreal Engine Blueprints.
- **Pins**: Inputs (left), outputs (right), flow pins (top/bottom or left/right for in/out), value pins (typed, color-coded).
- **Fields**: Node properties editable in the inspector, used as defaults if input pins are unconnected.
- **Flow**: Execution order is explicit via flow pins and links, not implicit by layout.
- **Pure nodes**: Data-only, no flow pins (e.g., math, struct get, copy).
- **Impure nodes**: Have flow pins, may mutate state or perform side effects.
- **Graph modularity**: Functions, macros, and event graphs are supported as sub-graphs.

## Phase 2 — Utility Nodes (Blueprint Parallels)

### Sqrt (Pure Node)
- **Pins**: `A` (Number, in), `Result` (Number, out)
- **Fields**: `A` (default 0.0)
- **Behavior**: Pure node, computes `Result = sqrt(A)`
- **Blueprint Analogue**: Math node (e.g., "Sqrt (float)")

### Random Int (Pure Node)
- **Pins**: `Min` (Number, in), `Max` (Number, in), `Result` (Number, out)
- **Fields**: `Min` (0), `Max` (10)
- **Behavior**: Pure node, computes `Result = random(Min, Max)`
- **Blueprint Analogue**: "Random Integer in Range"

### Array Operations (Impure Nodes)
- **Array Push / PushFront / PushUnique**
  - **Pins**: `In` (Flow, in), `Array` (Array, in), `Value` (Any, in), `Out` (Flow, out)
  - **Behavior**: Mutates array, adds value at end/front/unique
  - **Blueprint Analogue**: "Add", "Add Unique", "Insert"
- **Array Clear**
  - **Pins**: `In` (Flow, in), `Array` (Array, in), `Out` (Flow, out)
  - **Behavior**: Clears array
  - **Blueprint Analogue**: "Clear Array"
- **Array Resize**
  - **Pins**: `In` (Flow, in), `Array` (Array, in), `Size` (Number, in), `Out` (Flow, out)
  - **Behavior**: Resizes array
  - **Blueprint Analogue**: "Set Array Length"

## Phase 3 — Struct Access Nodes

### Struct Get Field (Pure Node)
- **Pins**: `Object` (Struct, in), `Value` (Any, out)
- **Fields**: `Field Name` (string, default "x")
- **Behavior**: Pure node, outputs `Object.FieldName`
- **Blueprint Analogue**: "Break Struct" or member access node

### Struct Set Field (Impure Node)
- **Pins**: `In` (Flow, in), `Object` (Struct, in), `Value` (Any, in), `Out` (Flow, out)
- **Fields**: `Field Name` (string, default "x")
- **Behavior**: Sets `Object.FieldName = Value`
- **Blueprint Analogue**: "Set Member in Struct"

### Copy (Pure Node)
- **Pins**: `Value` (Any, in), `Result` (Any, out)
- **Behavior**: Pure node, outputs a copy of the input
- **Blueprint Analogue**: "Copy" node

## Phase 4 — Struct Definitions and Initialization
- Use `StructDefine` nodes at the start of the graph to declare types (e.g., Coord, AStarNode)
- Use `StructCreate` nodes to instantiate values
- Use `ArrayPush` to build arrays of structs (e.g., Actions array)
- **Blueprint Analogue**: Struct definition and construction nodes

## Phase 5 — Helper Functions as Sub-Graphs
- Each helper (e.g., `equal`, `GetCost`, `ValidPreCheck`, `Act`, `transition`, `calculateCost`, `getNextLegalStates`, `findPath`) is authored as a Function node with its own graph
- Use only existing node types; compose logic visually
- **Blueprint Analogue**: Function graphs, macro graphs

## Phase 6 — Main Game Loop Graph
- Top-level flow: [Start] → [ClearGrid] → [StructDefine...] → [Variable Set...] → [While/Ticker]
- Use [Branch], [ForEach], [Function Call], [DrawCell], [RenderFrame], [Delay] to implement loop logic
- Animate agent, path, and goal as in Blueprint event graphs
- **Blueprint Analogue**: Event Graph (Tick/Event BeginPlay), main loop

## Authoring Best Practices
- Prefer pure nodes for math/data, impure for flow/state
- Use fields for defaults, pins for overrides
- Compose complex logic from small, reusable nodes
- Test each function/sub-graph in isolation before integrating
- Use color-coding and pin types for clarity
- Document node purpose and expected connections in the editor

---
