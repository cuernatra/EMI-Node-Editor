## How the Demo Connects the Node Graph, Renderer, and Native Bridge

The demo operates as a pipeline connecting the visual node graph, the rendering system, and the C++ demo logic via native calls. Here’s how the pieces fit together:

1. **Visual Node Graph (graph.txt):**
   - The user edits or loads a node graph that describes the logic and flow of the demo (e.g., A* steps, rendering, timing).
   - Nodes like `NativeCall`, `DrawCell`, `ClearGrid`, and `RenderFrame` represent actions or bridge points to native code.

2. **Graph Compilation and Execution:**
   - The node graph is compiled to EMI-Script AST using the graph compiler.
   - The compiled AST is executed in the EMI VM, which interprets the logic and triggers native calls as needed.

3. **Native Bridge (previewNatives.cpp):**
   - Functions such as `drawcell`, `cleargrid`, `rendergrid`, and all `astar_*` routines are registered with the VM using `EMI_REGISTER`.
   - When a `NativeCall` node is executed in the graph, it invokes the corresponding C++ function via the bridge.
   - This is how the graph triggers A* initialization, stepping, rendering, and interaction logic.

4. **Renderer (renderPanel.cpp, graphPreviewPanel.cpp):**
   - The renderer is responsible for displaying the current state of the pixel buffer or grid.
   - It does not know about A* or demo logic; it simply renders what the native bridge and graph logic produce.
   - The pixel buffer is updated by native bridge functions (e.g., `drawcell`), and the renderer displays it each frame.

5. **Demo Logic Separation:**
   - All demo-specific logic (A* state, agent movement, pathfinding, etc.) is encapsulated in native functions and invoked only via the node graph.
   - The renderer panels are generic and only visualize the output; they do not contain demo-specific code.

6. **Flow Example:**
   - The graph starts with a `Start` node, then uses a `NativeCall` to `astar_init` to set up the grid.
   - A `Ticker` or loop node triggers periodic `NativeCall` executions to `astar_step` and `astar_render`.
   - Rendering nodes (`DrawCell`, `RenderFrame`) update the pixel buffer, which the renderer displays.
   - All communication between the graph, demo logic, and renderer is through well-defined native calls and the pixel buffer.
