# core/registry/nodes/

This is where you work when adding or editing nodes.

## Files

| File | Nodes inside |
|---|---|
| `dataNodes.cpp` | Constants, variables, arrays, debug print |
| `logicNodes.cpp` | Arithmetic, comparison, logic, not |
| `flowNodes.cpp` | Start, branch, loop, forEach, sequence, delay, call function |
| `renderNodes.cpp` | Drawing/render nodes |
| `eventNodes.cpp` | Event trigger nodes |
| `structNodes.cpp` | Struct definition and creation |
| `demoNodes.cpp` | Sandbox/demo nodes |

Put your new node in whichever file fits. If nothing fits, create a new `*Nodes.cpp` and add a `Register*Nodes()` call.

## How to add a node (complete example)

```cpp
#include "../nodeRegistry.h"
#include "../../compiler/nodeCompileHelpers.h"   // gives you all helpers

namespace {

Node* CompileMyAddNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* pinA = FindInputPin(n, "A");
    const Pin* pinB = FindInputPin(n, "B");
    if (!pinA || !pinB) { compiler->Error("MyAdd needs A and B"); return nullptr; }

    Node* lhs = BuildNumberOperand(compiler, n, *pinA);
    Node* rhs = BuildNumberOperand(compiler, n, *pinB);
    return MakeBinaryOpNode(Token::Add, lhs, rhs);
}

} // namespace

void NodeRegistry::RegisterMyNodes()   // declare this in nodeRegistry.h
{
    Register(NodeDescriptor{
        .type  = NodeType::MyAdd,      // add this to graph/types.h first
        .label = "My Add",
        .pins  = {
            { "In",     PinType::Flow,   true  },
            { "A",      PinType::Number, true  },
            { "B",      PinType::Number, true  },
            { "Out",    PinType::Flow,   false },
            { "Result", PinType::Number, false },
        },
        .fields = {
            { "A", PinType::Number, "0.0" },
            { "B", PinType::Number, "0.0" },
        },
        .compile           = CompileMyAddNode,
        .category          = "Math",
        .saveToken         = "MyAdd",
        .deferredInputPins = { "A", "B" },
        .renderStyle       = NodeRenderStyle::Binary,
    });
}
```

Steps:
1. Add `NodeType::MyAdd` to `graph/types.h`.
2. Write the compile function.
3. Write the `NodeDescriptor` and call `Register()`.
4. Declare `RegisterMyNodes()` in `nodeRegistry.h` and call it from `nodeRegistry.cpp`.

## Which callback to use

| Callback | Use when |
|---|---|
| `compile` | Node produces a value or a single statement (most nodes) |
| `compileFlow` | Node controls what runs next: if/loop/sequence (see `compiler/README.md`) |
| `compileOutput` | Different output pins carry different values (e.g. Loop → Index vs LastIndex) |
| `compilePrelude` | Node needs a variable declared before the main flow runs (e.g. Variable, Loop) |
| `compileTopLevel` | Node emits a top-level definition like a function |
| `deserialize` | Node has unusual pins or a legacy save format |

When unsure, use `compile`.

## Available helpers (from ../../compiler/nodeCompileHelpers.h)

**Find pins and fields:**
- `FindInputPin(n, "Name")` — find an input pin by name
- `FindOutputPin(n, "Name")` — find an output pin by name
- `FindField(n, "Name")` — read a field value string

**Build inputs (wired → compile upstream, unwired → read field):**
- `BuildNumberOperand(compiler, n, pin)` — number
- `BuildBoolOperand(compiler, n, pin)` — boolean
- `BuildArrayInput(compiler, n, pin)` — array

**Build AST nodes directly:**
- `MakeNode(Token::...)` — any token type
- `MakeNumberLiteral(1.5)`, `MakeBoolLiteral(true)`, `MakeStringLiteral("hi")`
- `MakeIdNode("varName")` — a variable or function name
- `MakeBinaryOpNode(Token::Add, lhs, rhs)` — binary expression
- `MakeUnaryOpNode(Token::Not, operand)` — unary expression
- `MakeFunctionCallNode("Array.Push", { arrayExpr, valueExpr })` — function call
- `MakeIndexerNode(arrayExpr, indexExpr)` — array index

**Token → string field:**
- `ParseOperatorToken(FindField(n, "Op"))` — "+"/"-"/"*"/"/" → Token
- `ParseComparisonToken(...)` — "=="/"!="/"<" etc. → Token
- `ParseLogicToken(...)` — "AND"/"OR" → Token

## Rules

- **Errors:** call `compiler->Error("message")` and return `nullptr`. Keep messages user-readable.
- **Ownership:** if you create `Node*` objects and return early due to an error, `delete` everything not yet attached to a parent.
- **No compiler changes:** if you need something that doesn't exist in the helpers, add it to `nodeCompileHelpers.h`, not to `graphCompiler.*`.
