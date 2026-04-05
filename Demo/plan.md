## How the Demo Connects the Node Graph, Renderer, and Native Bridge

The demo operates as a pipeline connecting the visual node graph, the rendering system, and the C++ demo logic via native calls. Here's how the pieces fit together:

1. **Visual Node Graph (graph.txt):**
   - The user edits or loads a node graph that describes the logic and flow of the demo (e.g., A* steps, rendering, timing).
   - Nodes like `NativeCall`, `DrawCell`, `ClearGrid`, and `RenderGrid` represent actions or bridge points to native code.

2. **Graph Compilation and Execution:**
   - The node graph is compiled to EMI-Script AST using the graph compiler.
   - The compiled AST is executed in the EMI VM, which interprets the logic and triggers native calls as needed.

3. **Native Bridge (previewNatives.cpp):**
   - Functions such as `astar_init`, `astar_step`, `astar_draw`, and `astar_interact` are registered with the VM using `EMI_REGISTER`.
   - When a `NativeCall` node is executed in the graph, it invokes the corresponding C++ function via the bridge.
   - This is how the graph triggers A* initialization, stepping, drawing, and interaction logic.

4. **Renderer (renderPanel.cpp, graphPreviewPanel.cpp):**
   - The renderer is responsible for displaying the current state of the pixel buffer or grid.
   - It does not know about A* or demo logic; it simply renders what the native bridge and graph logic produce.
   - The pixel buffer is updated by native bridge functions (e.g., `drawcell`), and the renderer displays it each frame.

5. **Demo Logic Separation:**
   - All demo-specific logic (A* state, agent movement, pathfinding, etc.) is encapsulated in native functions and invoked only via the node graph.
   - The renderer panels are generic and only visualize the output; they do not contain demo-specific code.

6. **Flow Example (current graph):**
   - `Start` → `astar_init` sets up the grid and initial path.
   - `Ticker` drives a loop: `astar_step` moves the agent, `astar_interact` handles mouse input.
   - `ClearGrid` → `astar_draw` → `RenderGrid` forms the explicit render pipeline.
   - `NativeGet(astar_get)` and `NativeGet(astar_needs_retarget)` sit as data nodes showing readable state.

---

## Migration Plan: previewNatives → Graph-Only Logic

**Goal:** All A* demo logic lives in the visual graph using only the primitive functions from
`NodeGameFunctions.cpp` (drawing, input, random). `previewNatives.cpp` is removed entirely.

### Primitive functions that will remain (NodeGameFunctions.cpp)

| Native | Purpose |
|--------|---------|
| `drawcell(x, y, r, g, b)` | Draw one grid cell |
| `cleargrid(w, h)` | Clear the render buffer |
| `rendergrid()` | Flush the render buffer |
| `getpixel(x, y)` | Read a pixel value |
| `getmousex()` / `getmousey()` | Mouse position |
| `ismousebuttondown(btn)` | Mouse button state |
| `iskeydown(key)` | Keyboard state |
| `random(lo, hi)` | Random integer |

---

### Phase 1 — Render pipeline explicit in graph *(complete)*

**What changed:**
- `astar_render` split: `DrawCells()` helper added, `astar_draw` registered as a new native (draw only, no clear/flush).
- New typed nodes `ClearGrid`, `DrawCell`, `RenderGrid` in `renderNodes.cpp` compile to the actual native calls.
- Graph: `ClearGrid(20,20) → NativeCall(astar_draw) → RenderGrid` replaces `NativeCall(astar_render)`.
- `NativeGet(astar_get, key=0)` and `NativeGet(astar_needs_retarget)` visible as data nodes.

**Removed from previewNatives:** `astar_render` is no longer called by the graph (kept compiled for safety).

**Verify:** Demo runs; agent moves; walls/path/agent/goal all render correctly.

---

### Phase 2 — Mouse interaction in graph

**Goal:** Replace `NativeCall(astar_interact)` with explicit graph logic using input primitives.

**New thin bridge natives to add to previewNatives.cpp:**
- `astar_getwall(x, y)` → number — reads `s_astar.walls[y*w+x]` (0 or 1).
- `astar_setwall(x, y, isWall)` — writes the wall value and recomputes the path (same logic as the left/right click branches inside `astar_interact_impl`).

**Graph changes:**
- `NativeGet(getmousex)` → Variable(mouseX)
- `NativeGet(getmousey)` → Variable(mouseY)
- `NativeGet(ismousebuttondown, 0)` → Variable(leftBtn)
- `NativeGet(ismousebuttondown, 1)` → Variable(rightBtn)
- `Branch(leftBtn)` True → `NativeCall(astar_setwall, mouseX, mouseY, 1)`
- `Branch(rightBtn)` True → `NativeCall(astar_setwall, mouseX, mouseY, 0)`

**Removed from previewNatives:** `astar_interact_impl` and `EMI_REGISTER(astar_interact, ...)`.

**Verify:** Left-click places walls; right-click erases walls; path recomputes after each edit.

---

### Phase 3 — Drawing loop in graph

**Goal:** Replace `NativeCall(astar_draw)` with a graph loop that calls `DrawCell` directly.

**New thin bridge natives to add to previewNatives.cpp:**
- `astar_getwall(x, y)` (from Phase 2, already present).
- `astar_getpathcell_x(index)` → number — x coordinate of path cell at index.
- `astar_getpathcell_y(index)` → number — y coordinate of path cell at index.
- `astar_getpathlen()` → number — number of remaining path cells (`path.size() - pathStep`).
- `astar_get(0..3)` (already present: agentX, agentY, goalX, goalY).

**Graph changes:**
- `Loop(0 .. gridW)` × `Loop(0 .. gridH)`: `NativeGet(astar_getwall, x, y)` → `Branch` → `DrawCell(x, y, 75, 75, 75)` (walls grey).
- `Loop(0 .. astar_getpathlen)`: `DrawCell(astar_getpathcell_x(i), astar_getpathcell_y(i), 0, 100, 220)` (path blue).
- `DrawCell(goalX, goalY, 0, 180, 60)` (goal green).
- `DrawCell(agentX, agentY, 220, 50, 50)` (agent red).

**Removed from previewNatives:** `astar_draw_impl`, `DrawCells()`, `DrawFrame()`.

**Verify:** Grid renders identically to before. Frame rate may drop slightly due to graph loop overhead; tune if needed.

---

### Phase 4 — Agent movement in graph

**Goal:** Replace `NativeCall(astar_step)` with graph-visible path stepping.

**New thin bridge natives to add to previewNatives.cpp:**
- `astar_getpathcell_x(index)`, `astar_getpathcell_y(index)` (from Phase 3, already present).
- `astar_getpathlen()` (from Phase 3, already present).
- `astar_setagent(x, y)` — writes `s_astar.agentX/Y` (does not recompute path).
- `astar_setpathstep(step)` — writes `s_astar.pathStep`.

**Graph changes:**
- Variable(pathStep) starts at 1.
- Each tick: `Branch(pathStep >= astar_getpathlen())`:
  - True → `NativeCall(astar_retarget)` then reset pathStep Variable to 1.
  - False → `NativeGet(astar_getpathcell_x, pathStep)` → Variable(agentX); same for Y; `NativeCall(astar_setagent, agentX, agentY)`; increment pathStep.

**Removed from previewNatives:** `astar_step_impl`.

**Verify:** Agent walks along path, picks new goal on arrival. pathStep Variable visible in graph inspector.

---

### Phase 5 — Retarget and path computation in graph

**Goal:** Replace `NativeCall(astar_retarget)` with graph-visible goal selection.

**New thin bridge natives to add to previewNatives.cpp:**
- `astar_findpath(startX, startY, goalX, goalY)` → number — runs A* and stores result path; returns path length (0 if no path found). C++ still owns the algorithm.
- `astar_getwall(x, y)` (from Phase 2, already present).

**Graph changes:**
- Variable(goalX), Variable(goalY) now graph-owned.
- Retarget loop: `random(0, gridW-1)` → Variable(candidateX); same for Y; `Branch(astar_getwall(candidateX, candidateY) == 0)` → `NativeCall(astar_findpath, agentX, agentY, candidateX, candidateY)` → `Branch(result > 0)` → accept goal, reset pathStep.
- Repeat until valid goal found (While loop with attempt counter).

**Removed from previewNatives:** `astar_retarget_impl`, `Retarget()`.

**Verify:** Agent still finds new goals. Random seed may differ from before; verify path is always valid.

---

### Phase 6 — Wall generation in graph

**Goal:** Replace `astar_init`'s wall generation with graph loops.

**Bridge natives still needed:** `astar_setwall(x, y, val)` (Phase 2), `astar_findpath(...)` (Phase 5), `astar_setagent(x, y)` (Phase 4).

**Graph changes:**
- After init, `Loop(0..gridH)` × `Loop(0..gridW)`:
  - `Branch(random(0, 4) == 0)` → `NativeCall(astar_setwall, x, y, 1)`.
- Clear start/goal cells: `NativeCall(astar_setwall, startX, startY, 0)` and `NativeCall(astar_setwall, goalX, goalY, 0)`.
- Verify connectivity: `NativeCall(astar_findpath, startX, startY, goalX, goalY)` → `Branch(result == 0)` → fallback: clear all walls, retry.

**Reduces astar_init to:** grid dimension setup only (setting `s_astar.w`, `s_astar.h`, `s_astar.initialized = true`). Wall generation is now fully graph-owned.

**Verify:** Demo starts correctly with randomly generated passable maze.

---

### Phase 7 — A* algorithm in graph *(final)*

**Goal:** Implement A* path search in the visual graph using EMI-Script primitives. Remove `previewNatives.cpp` entirely.

**Replaces:** `astar_findpath(...)` (the last remaining computation native).

**Graph state (Variables/Arrays):**
- `walls[]` — flat array of gridW×gridH values (was C++ vector).
- `openList[]`, `closedList[]` — arrays of {x, y, g, f, parentIndex} structs.
- `path[]` — result path as array of {x, y}.

**Graph logic:**
- Priority queue via sorted array insert (ForEach + Comparison).
- Neighbour expansion: 4-directional with bounds check (Comparison nodes).
- Heuristic: Manhattan distance (Operator: `abs(dx) + abs(dy)` via graph math).
- Path reconstruction: walk parentIndex chain back to start, prepend to path array.

**This phase ports `game.ril`'s `findPath` function to the visual graph verbatim.**

**Removed from previewNatives:** Everything. File deleted.

**Verify:** Full A* demo runs using only `NodeGameFunctions.cpp` natives and graph logic. Performance validated at target FPS.

---

### Bridge native inventory by phase

| Native | Added in | Removed in | Lives in |
|--------|----------|------------|----------|
| `astar_init` (grid dims only) | existing | Phase 7 | previewNatives |
| `astar_step` | existing | Phase 4 | previewNatives |
| `astar_render` | existing | Phase 1 | previewNatives |
| `astar_draw` | Phase 1 | Phase 3 | previewNatives |
| `astar_interact` | existing | Phase 2 | previewNatives |
| `astar_retarget` | existing | Phase 5 | previewNatives |
| `astar_get` | existing | Phase 4/5 | previewNatives |
| `astar_needs_retarget` | existing | Phase 4 | previewNatives |
| `astar_getwall` | Phase 2 | Phase 7 | previewNatives |
| `astar_setwall` | Phase 2 | Phase 7 | previewNatives |
| `astar_getpathcell_x/y` | Phase 3 | Phase 7 | previewNatives |
| `astar_getpathlen` | Phase 3 | Phase 7 | previewNatives |
| `astar_setagent` | Phase 4 | Phase 7 | previewNatives |
| `astar_setpathstep` | Phase 4 | Phase 7 | previewNatives |
| `astar_findpath` | Phase 5 | Phase 7 | previewNatives |
| `drawcell` | existing | never | NodeGameFunctions |
| `cleargrid` | existing | never | NodeGameFunctions |
| `rendergrid` | existing | never | NodeGameFunctions |
| `getmousex/y` | existing | never | NodeGameFunctions |
| `ismousebuttondown` | existing | never | NodeGameFunctions |
| `random` | existing | never | NodeGameFunctions |
