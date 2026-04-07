# Graph compiler

This folder contains the code that compiles the *visual graph* (boxes + wires) into an EMI-Script AST.

In plain language: it answers two questions:
- **Flow wires**: “what runs next?” (statement order)
- **Data wires**: “what value is used?” (expressions)

## Where to start

- `graphCompiler.h/.cpp` — the main entry point (`GraphCompiler::Compile`)
- `pinResolver.*` — builds fast lookups so compilation can ask “what is connected to what?”
- `nodeCompileHelpers.h` — the helper toolbox used by node compile callbacks

## What you normally change (and what you normally don’t)

- Normal node work: you usually **do not** edit this folder.
	Instead: edit `src/core/registry/nodes/*.cpp` and use the helpers.
- Add compiler features only when many nodes need the same new capability.

## Rule

Avoid adding node-type special cases in the compiler. Keep node semantics in descriptors.
