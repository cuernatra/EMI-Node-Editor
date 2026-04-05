# Node Editor — Incremental Native-to-Node Migration Plan

## Vision

Move functionality from hand-written `previewNatives.cpp` into real visual node
types that compile to EMI AST — taking design cues from Unreal Engine Blueprints.
The demo must not break at any phase boundary.

---

## Blueprints Patterns Applied Here

| Blueprints concept | Our equivalent | Phase |
|---|---|---|
| **Latent / async node** (Set Timer by Function) | `Ticker` — while+delay in one node | 2 |
| **Input Action event** (On Mouse Button Pressed) | `OnMouseButton` — polls + routes flow | 3 |
| **Pure function node** (Get Actor Location) | `AStarGetState` — value out, no flow | 3 |
| **Composite AI action** (AI Move To) | `AStarFrame` — step+render+branch in one | 4 |
| **K2 node with typed pins** | `AStarInit`, `AStarSetWall` typed wrappers | 5 |
| **Draw call nodes** (Draw Debug Sphere) | Fix `DrawGrid` / `DrawRect` to actually compile | 1 |

---

## Current State (baseline, working demo)

```
[Start]
  → [NativeCall: astar_init  (6 args)]
  → [While] ← [Constant: true]
       Body →
         [NativeCall: astar_step]
         → [NativeCall: astar_render]
         → [NativeCall: astar_interact]
         → [Delay: 50 ms]

Total: 8 nodes   NativeCall nodes: 3   "magic string" function names: 4
```

---

## Target State (after all phases)

```
[Start]
  → [AStarInit  W=20 H=20  SX=0 SY=0  GX=15 GY=15]
  → [Ticker  FPS=20]
       Tick →
         [AStarFrame]
           Out →
             [OnMouseButton]
               Left  → [AStarSetWall  Wall=true]
               Right → [AStarSetWall  Wall=false]

Total: 7 nodes   NativeCall nodes: 0   magic strings: 0
```

---

## Phase 1 — Fix DrawGrid / DrawRect (compile to real calls)

**Status of these nodes today:** both register `CompileScopeNode` which emits
`Token::Scope` — a no-op. They render visually but produce no runtime code.

### DrawGrid  
*Blueprint analogue: Flush Persistent Debug Lines (clears + resets)*

Compile to: `cleargrid(W, H)`  
Files: `src/core/registry/nodes/eventNodes.cpp` (replace `CompileScopeNode`)

```cpp
Node* CompileDrawGridNode(GraphCompiler* compiler, const VisualNode& n)
{
    Node* w = BuildNumberOperand(compiler, n, *FindInputPin(n, "W"), "W");
    Node* h = BuildNumberOperand(compiler, n, *FindInputPin(n, "H"), "H");
    return compiler->EmitFunctionCall("cleargrid", { w, h });
}
```

### DrawRect  
*Blueprint analogue: Draw Debug Box (draw at position with colour)*

Compile to: `drawcell(X, Y, R, G, B)` (W=H=1 fast path).  
For W×H > 1 cells, later phase can emit a nested For loop.  
Files: `src/core/registry/nodes/eventNodes.cpp`

```cpp
Node* CompileDrawRectNode(GraphCompiler* compiler, const VisualNode& n)
{
    // Build args from input pins, fall back to field defaults.
    Node* x = BuildNumberOperand(..., "X");
    Node* y = BuildNumberOperand(..., "Y");
    Node* r = BuildNumberOperand(..., "R");
    Node* g = BuildNumberOperand(..., "G");
    Node* b = BuildNumberOperand(..., "B");
    return compiler->EmitFunctionCall("drawcell", { x, y, r, g, b });
}
```

**Demo impact:** none (demo uses NativeCall, not DrawGrid/DrawRect).  
Other graphs that use DrawGrid/DrawRect nodes now actually work.

---

## Phase 2 — Ticker node  
*Blueprint analogue: Set Timer by Function / Event Tick*

Replaces the **While + Constant(true) + Delay** pattern (3 nodes → 1).

### Pins

| Pin | Direction | Type | Default |
|---|---|---|---|
| In | in | Flow | — |
| FPS | in | Number | 20 |
| Tick | out | Flow | — |
| Stop | out | Flow | — |

`Stop` fires after the loop if the condition is externally broken (future use).

### Compiled AST

```
while (true) {
    [Tick body chain]
    delay(1000.0 / FPS)
}
[Stop chain]
```

Implementation mirrors `NodeType::While` in `graphCompiler.cpp`:

```cpp
case NodeType::Ticker:
{
    Node* fps  = BuildFpsExpr(compiler, n);   // pin or field default
    Node* body = MakeNode(Token::Scope);
    Node* loop = MakeNode(Token::While);
    loop->children.push_back(MakeBoolLiteral(true));
    loop->children.push_back(body);

    if (const Pin* tick = GetOutputPinByName(n, "Tick"))
        AppendFlowChainFromOutput(tick->id, body);

    // Delay appended *after* user's body chain, inside the loop scope.
    body->children.push_back(
        compiler->EmitFunctionCall("delay",
            { MakeDiv(MakeNumberLiteral(1000.0), fps) }));

    targetScope->children.push_back(loop);

    if (const Pin* stop = GetOutputPinByName(n, "Stop"))
        AppendFlowChainFromOutput(stop->id, targetScope);
    return;
}
```

### Files to change
- `src/core/graph/types.h` — add `NodeType::Ticker`
- `src/core/registry/nodes/flowNodes.cpp` — register + `CompileTickerNode`
- `src/core/compiler/graphCompiler.cpp` — `case NodeType::Ticker` in `AppendFlowNode`

### Demo graph change
Remove: `While`, `Constant(true)`, `Delay`  
Add: `Ticker(FPS=20)`  
Net: **−2 nodes**

---

## Phase 3 — OnMouseButton + AStarGetState  

### 3a. OnMouseButton  
*Blueprint analogue: InputAction node (Left / Right / None branching)*

Polls `Input.IsMouseButtonDown` and `Input.GetMouseX/Y` each time it is reached
in the flow, routing to `Left`, `Right`, or `Pass`. Exposes `CellX` / `CellY`
as value output pins (same pattern as `Loop`→`Index`).

#### Pins

| Pin | Direction | Type |
|---|---|---|
| In | in | Flow |
| Left | out | Flow |
| Right | out | Flow |
| Pass | out | Flow |
| CellX | out | Number |
| CellY | out | Number |

#### Compiled AST

```
var __mb_cellX_<id> = Input.GetMouseX()
var __mb_cellY_<id> = Input.GetMouseY()
if (Input.IsMouseButtonDown(0)) {
    [Left chain]
} else if (Input.IsMouseButtonDown(1)) {
    [Right chain]
} else {
    [Pass chain]
}
```

When a downstream node reads `CellX` / `CellY`, `BuildNode(n, outPinIdx)` returns
`MakeIdNode("__mb_cellX_<nodeId>")` — same mechanism as `Loop`→`Index`.

#### Files to change
- `src/core/graph/types.h` — add `NodeType::OnMouseButton`
- `src/core/registry/nodes/eventNodes.cpp` — register node
- `src/core/compiler/graphCompiler.cpp`:
  - `AppendFlowNode`: handle `NodeType::OnMouseButton` (emit declarations + if-chain)
  - `BuildNode`: return cell variable identifiers for `CellX` / `CellY` output pins

### 3b. AStarGetState (pure node)  
*Blueprint analogue: Get Actor Location (pure getter, no flow pins)*

Wraps `astar_get(key)` with named outputs instead of a magic key integer.

#### Pins

| Pin | Direction | Type |
|---|---|---|
| AgentX | out | Number |
| AgentY | out | Number |
| GoalX | out | Number |
| GoalY | out | Number |
| PathLen | out | Number |

Each output pin compiles to `astar_get(<key>)` when read.

```cpp
// In BuildNode special case for NodeType::AStarGetState:
if (pin.name == "AgentX") return EmitFunctionCall("astar_get", { MakeNumberLiteral(0) });
if (pin.name == "AgentY") return EmitFunctionCall("astar_get", { MakeNumberLiteral(1) });
// ...
```

No flow pins → pure node, zero cost when outputs are not read.

#### Files to change
- `src/core/graph/types.h` — add `NodeType::AStarGetState`
- `src/core/registry/nodes/demoNodes.cpp` — register
- `src/core/compiler/graphCompiler.cpp` — `BuildNode` special case

### Demo graph change
Replace: `NativeCall(astar_interact)` with `OnMouseButton` → two `NativeCall(astar_set_wall)` calls  
Prerequisite: `astar_set_wall(x, y, isWall)` native added to `previewNatives.cpp` (extracted from `astar_interact_impl`)  
Net: **+1 node** (3→2 NativeCall removed, but two typed calls added) — demo is more readable

---

## Phase 4 — AStarFrame node  
*Blueprint analogue: AI Move To (composite action that steps the agent and branches on arrival)*

Replaces `NativeCall(astar_step)` + `NativeCall(astar_render)` (2 nodes → 1).
Adds a `Reached` output that fires the frame the agent arrives at the goal.

#### Pins

| Pin | Direction | Type |
|---|---|---|
| In | in | Flow |
| Out | out | Flow |
| Reached | out | Flow |

#### Compiled AST

```
astar_step()
astar_render()
if (astar_needs_retarget()) {
    [Reached chain]
} else {
    [Out chain]
}
```

`astar_needs_retarget()` is `true` immediately after `astar_step()` retargets
(i.e., the frame the old goal was consumed and a new path was computed).

#### Files to change
- `src/core/graph/types.h` — add `NodeType::AStarFrame`
- `src/core/registry/nodes/demoNodes.cpp` — register
- `src/core/compiler/graphCompiler.cpp` — `AppendFlowNode` case

### Demo graph change
Replace: `NativeCall(astar_step)` + `NativeCall(astar_render)` → `AStarFrame`  
Net: **−1 node**

---

## Phase 5 — AStarInit and AStarSetWall typed nodes  
*Blueprint analogue: K2 typed wrappers (strongly-typed function call nodes)*

These are purely about **editor clarity** — they compile to the same native calls
as NativeCall but with named, typed pins and no magic string field.

### AStarInit

| Pin | Direction | Type | Default |
|---|---|---|---|
| In | in | Flow | — |
| Width / Height | in | Number | 20 |
| StartX / StartY | in | Number | 0 |
| GoalX / GoalY | in | Number | 15 |
| Out | out | Flow | — |

Compiles to: `astar_init(Width, Height, StartX, StartY, GoalX, GoalY)`

### AStarSetWall

| Pin | Direction | Type | Default |
|---|---|---|---|
| In | in | Flow | — |
| X / Y | in | Number | 0 |
| Wall | in | Boolean | true |
| Out | out | Flow | — |

Compiles to: `astar_set_wall(X, Y, Wall)`  
Requires: `astar_set_wall` native in `previewNatives.cpp` (from Phase 3).

#### Files to change
- `src/core/graph/types.h` — add two NodeTypes
- `src/core/registry/nodes/demoNodes.cpp` — register both

### Demo graph change
Replace: `NativeCall(astar_init)` → `AStarInit`  
Replace: two `NativeCall(astar_set_wall)` → two `AStarSetWall`  
Net: **0 nodes gained** (3→3), but **zero NativeCall nodes remain**

---

## Summary of Node Count Across Phases

| Phase | Nodes | NativeCall | Notes |
|---|---|---|---|
| Baseline (now) | 8 | 3 | While+Constant+Delay, astar_* as strings |
| Phase 1 | 8 | 3 | DrawGrid/DrawRect fixed; demo unchanged |
| Phase 2 | 6 | 3 | Ticker replaces While+Constant+Delay |
| Phase 3 | 7 | 2 | OnMouseButton replaces astar_interact |
| Phase 4 | 6 | 2 | AStarFrame replaces step+render |
| Phase 5 | 6 | 0 | All calls become typed nodes |

---

## Native Layer After All Phases

| Function | Still in previewNatives? | Reason |
|---|---|---|
| `astar_init` | yes | called by `AStarInit` node |
| `astar_step` | yes | called by `AStarFrame` node |
| `astar_render` | yes | called by `AStarFrame` node |
| `astar_needs_retarget` | yes | called by `AStarFrame` node |
| `astar_set_wall` | yes (add in Phase 3) | called by `AStarSetWall` node |
| `astar_retarget` | yes | utility, may be exposed as node later |
| `astar_get` | yes | called by `AStarGetState` pure node |
| `astar_interact` | **remove** in Phase 3 | replaced by OnMouseButton + AStarSetWall |
| `astarpathfinder` | keep as convenience | single-node shortcut for full demo |

---

## Implementation Order (what to do within each phase)

1. Add `NodeType` enum value (`types.h`)
2. Add compile handler in `graphCompiler.cpp` if it needs flow routing  
3. Register the node descriptor in the relevant `*Nodes.cpp`
4. Add any new native to `previewNatives.cpp` and register with `EMI_REGISTER`
5. Update `demo.txt` to use the new node and verify the demo still runs
6. Update this document

Each step can be committed independently — the demo compiles and runs after
every step because old NativeCall nodes still work until explicitly removed.
