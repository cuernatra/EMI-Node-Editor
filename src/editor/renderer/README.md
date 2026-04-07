# Rendering

Drawing code for the node editor:
- pins, links, nodes
- field widgets

If you are trying to change *layout* or *rules* for how nodes appear, start in `nodeRenderer.*`.
If you are trying to change a single widget (like a number input), start in `fieldWidgetRenderer.*`.

Files:
- `fieldWidgetRenderer.*` — reusable widgets (mostly no graph context)
- `nodeRenderer.*` — node-level orchestration and graph-aware layout decisions
- `pinRenderer.*`, `linkRenderer.*` — drawing primitives
