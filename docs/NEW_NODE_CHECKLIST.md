# New Node Checklist

Use this checklist when adding a new node.

## Normal node

- [ ] Add the new `NodeType` in `src/core/graph/types.h`.
- [ ] Add the node descriptor in the correct file under `src/core/registry/nodes/`.
- [ ] Add a named compile callback in that same file.
- [ ] Set `saveToken` explicitly.
- [ ] Keep `deserialize = nullptr` if the pin layout is fixed.
- [ ] Build the project.
- [ ] Run the tests.
- [ ] Verify `saveToken` resolves back to the same `NodeType` through the registry.
- [ ] Verify the node appears in the palette.
- [ ] Verify the node saves and loads correctly.

## Dynamic-pin node

- [ ] Complete the normal node steps above.
- [ ] Add a named deserialize callback in the same category file.
- [ ] Verify the callback rebuilds the pin layout correctly.
- [ ] Verify save/load preserves the node shape.

## Rules

- Do not add a new compiler method unless the existing helpers cannot express the behavior.
- Do not add editor changes for a routine node.
- Do not rely on fallback save tokens.
- Keep the compile callback close to the descriptor.
- Keep error messages clear and specific.

## When to touch extra files

Extra files are only needed when the node changes behavior outside the standard pattern.

Examples:

- New runtime type support may require `src/core/graph/types.h`.
- Special UI behavior may require an editor file.
- New compiler capability may require `src/core/compiler/graphCompiler.cpp`.

If a normal node forces more than one category file plus tests, the design should be rechecked.
