# Editor panels

UI panels shown in the editor.

Typical pattern:
- each `*Panel.cpp` owns one panel’s layout and interaction
- panels should call into `editor/graph` and `core/` rather than re-implementing logic

Start here:
- `leftPanel.cpp` (palette)
- `inspectorPanel.cpp`
- `consolePanel.cpp`
