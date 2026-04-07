# Editor layer

Owns the interactive UI: graph editing, panels, rendering, save/load.

In plain language: this is the part you see and click.

Folders:
- `graph/` — graph editor behavior (save/load, refresh, compilation trigger)
- `panels/` — left/top/inspector/console/etc.
- `renderer/` — drawing nodes/pins/links + field widgets

## Where to start

- If you are debugging save/load: start in `graph/graphSerializer.*`.
- If you are debugging what happens when you press Run: start in `graph/graphCompilation.*`.
- If you are changing how nodes look: start in `renderer/`.

Rule: keep node semantics in `core/registry`, not in editor code.
