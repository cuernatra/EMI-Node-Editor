# Architecture & File Dependencies

## Core Data Structures

| File | Purpose | Key Types |
|------|---------|-----------|
| [pin.h](src/core/graph/pin.h) | Connection point on nodes | `Pin` (id, parentNodeId, type), `MakePin()` |
| [visualNode.h](src/core/graph/visualNode.h) | A node in the graph | `VisualNode` (id, pins, fields, position) |
| [link.h](src/core/graph/link.h) | Connection between pins | `Link` (startPinId, endPinId) |

## Node Type System

| File | Purpose | Key Types |
|------|---------|-----------|
| [nodeDescriptor.h](src/core/registry/nodeDescriptor.h) | Static template for node types | `NodeDescriptor`, `PinDescriptor`, `FieldDescriptor` |
| [nodeRegistry.h/cpp](src/core/registry/nodeRegistry.h) | Registry of all node types | `NodeRegistry` (singleton, holds descriptors) |
| [nodeFactory.h/cpp](src/core/registry/nodeFactory.h) | Creates nodes from descriptors | `CreateNodeFromType()`, `PopulateFromDescriptor()` |

## UI Components

| File | Purpose | Summary |
|------|---------|---------|
| [fieldWidget.h/cpp](src/core/registry/fieldWidget.h) | ImGui field editing | Number input, Boolean checkbox, String input, Operator dropdown |
| [graphEditor.h/cpp](src/editor/graphEditor.h) | Graph canvas & interaction | Node/link creation, deletion, dragging |
| [graphState.h/cpp](src/editor/graphState.h) | Graph state management | Stores nodes/links, ID generation, queries, dirty tracking, `FindPin()` API |
| [visualNode.cpp](src/core/graph/visualNode.cpp) | Pin rendering | `DrawPin()`, `DrawNode()` via imgui-node-editor |

## Compilation Pipeline

| File | Purpose | Summary |
|------|---------|-----------|
| [pinResolver.h/cpp](src/core/compiler/pinResolver.h) | Input → source mapping | `Resolve(ed::PinId)` returns PinSource (upstream node + index) |
| [graphCompiler.h/cpp](src/core/compiler/graphCompiler.h) | Visual nodes → AST | `BuildNode()` per descriptor, `BuildExpr()` recursive building, named pin access via `GetInputPinByName()` |
| [graphCompilation.h/cpp](src/editor/graphCompilation.h) | High-level orchestration | `Compile()` validates graph, builds AST, TODO: invokes EMI parser |

## Serialization

| File | Purpose | Summary |
|------|---------|-----------|
| [graphSerializer.h/cpp](src/editor/graphSerializer.h) | Save/load graphs | `Save()` writes node/link IDs and field values; `Load()` restores |

---

## Dependency Graph

```
┌─────────────────────────────────────────────────────────────┐
│                    User Interactions (UI)                   │
└───────┬───────────────────────────────────────┬─────────────┘
         │                                       │
      DropBar                                  GraphState
   (palette drag)                         (stores nodes/links)
         │                                       │
         └──────────────┬────────────────────────┘
                           │
                     GraphEditor
                     (canvas, drag)
           │                                    │
           └────────────────┬───────────────────┘
                            │
                    VisualNode + Pin + Link
                    (data structures)  ◄─────────────────┐
                            │                            │
                            │                    NodeRegistry
                            │                    (type templates)
                            │                            │
                            └────────────────┬───────────┘
                                             │
                                     NodeFactory
                                     (creates instances)
                                             │
                        ┌────────────────────┘
                        │
                        ▼
                  FieldWidget
              (edits field values)
              (operator dropdowns)

────────────────────────────────────────────────────────────

         GRAPH → COMPILATION PIPELINE
         
                  GraphState
                  (nodes + links)
                        │
                        ▼
                  PinResolver
              (build input→source map)
                        │
                        ▼
                  GraphCompiler
          (visual nodes → AST via descriptors)
                   BuildNode() calls
               descriptor callbacks
                        │
                        ▼
                  GraphCompilation
            (orchestrates ▲ returns AST)
              (TODO: invoke EMI Parser::ParseAST)
```

---

## Data Flow: Adding a Node Type

1. **Define type**: Add enum to `NodeType` in [types.h](src/core/graph/types.h)
2. **Register**: Add `NodeDescriptor` in [nodeRegistry.cpp](src/core/registry/nodeRegistry.cpp)
3. **Implement builder**: Add method to [graphCompiler.cpp](src/core/compiler/graphCompiler.cpp)
4. **Auto-handled**: Factory, UI, serialization all work via descriptor

## Data Flow: Creating a Node (UI)

1. **Palette drag**: [dropBar.h](src/ui/dropBar.h) creates a drag payload with `NodeType`
2. **Spawn request**: [graphEditor.h](src/editor/graphEditor.h) receives payload and requests a new node
3. **Factory build**: [nodeFactory.h](src/core/registry/nodeFactory.h) builds pins/fields from the descriptor
4. **State update**: [graphState.h](src/editor/graphState.h) stores the new `VisualNode`

## Data Flow: Compiling

`GraphCompilation::Compile()` → `PinResolver::Build()` → `GraphCompiler::BuildNode()` → `BuildExpr()` (recursive) → AST

---

## Guide: Mapping Visual Nodes to EMI AST

This guide explains how to connect visual nodes to EMI-Script's AST system for compilation.

### Overview

Visual nodes are compiled to EMI AST in [graphCompiler.cpp](src/core/compiler/graphCompiler.cpp). Each `NodeType` has a corresponding builder method that converts the visual node into an EMI AST node. The compiler uses **named pin access** (not positional) to avoid fragile dependencies on pin ordering.

### Files You'll Work With

| File | What You Do |
|------|-------------|
| [types.h](src/core/graph/types.h) | Add new `NodeType` enum value (if needed) |
| [nodeRegistry.cpp](src/core/registry/nodeRegistry.cpp) | Define node descriptor: pins, fields, display properties |
| [graphCompiler.h](src/core/compiler/graphCompiler.h) | Declare builder method (e.g., `BuildMyNode()`) |
| [graphCompiler.cpp](src/core/compiler/graphCompiler.cpp) | Implement builder: visual node → EMI AST node |
| EMI headers | Check [dependencies/emi/](dependencies/emi/) for AST node types you need |

### Step-by-Step: Adding a New Node Type

#### 1. Define the NodeType

Add your enum value to [types.h](src/core/graph/types.h):

```cpp
enum class NodeType {
    Constant, Operator, Comparison, Logic,
    MyNewNode,  // <-- Add here
    Unknown
};
```

Update `NodeTypeToString()` and `NodeTypeFromString()` in the same file.

#### 2. Register the Node Descriptor

In [nodeRegistry.cpp](src/core/registry/nodeRegistry.cpp), add a descriptor:

```cpp
NodeDescriptor myNewNodeDesc;
myNewNodeDesc.type = NodeType::MyNewNode;
myNewNodeDesc.name = "My Node";
myNewNodeDesc.color = ImColor(100, 200, 150);

// Define pins with NAMES (important for compiler!)
myNewNodeDesc.pins.push_back({"Input A", PinType::Number, true});
myNewNodeDesc.pins.push_back({"Input B", PinType::Number, true});
myNewNodeDesc.pins.push_back({"Result", PinType::Number, false});

// Optional: Add fields for user-editable values
myNewNodeDesc.fields.push_back({"Mode", FieldType::String, "default"});

registry[NodeType::MyNewNode] = myNewNodeDesc;
```

**Critical**: Pin names must be consistent - the compiler will look them up by name.

#### 3. Declare the Builder Method

In [graphCompiler.h](src/core/compiler/graphCompiler.h), add:

```cpp
private:
    Node* BuildMyNode(const VisualNode& node);
```

#### 4. Implement the Builder

In [graphCompiler.cpp](src/core/compiler/graphCompiler.cpp):

##### a) Add case to BuildNode()

```cpp
emi::ASTNode* GraphCompiler::BuildNode(const VisualNode& node) {
    switch (node.type) {
        case NodeType::Constant: return BuildConstant(node);
        case NodeType::Operator: return BuildOperator(node);
        case NodeType::MyNewNode: return BuildMyNode(node);  // <-- Add here
        // ...
    }
}
```

##### b) Implement the builder method

```cpp
Node* GraphCompiler::BuildMyNode(const VisualNode& node) {
    // 1. Get input pins by NAME (not index!)
    const Pin* pinA = GetInputPinByName(node, "Input A");
    const Pin* pinB = GetInputPinByName(node, "Input B");
    
    if (!pinA || !pinB) {
        Error("MyNode requires Input A and Input B pins");
        return nullptr;
    }
    
    // 2. Resolve inputs to their source expressions (RECURSIVE)
    Node* exprA = BuildExpr(*pinA);  // Compiles entire upstream graph
    Node* exprB = BuildExpr(*pinB);
    
    if (HasError || !exprA || !exprB) {
        // Cleanup on error
        if (exprA) delete exprA;
        if (exprB) delete exprB;
        return nullptr;
    }
    
    // 3. Access field values (if any)
    const std::string* modeStr = GetField(node, "Mode");
    std::string mode = modeStr ? *modeStr : "default";
    
    // 4. Create EMI AST node using the correct Token type
    // Look up Token enum in Parser/Lexer.h for the right type
    // Common pattern: Binary operation node
    Node* astNode = MakeNode(Token::Add);  // Replace with correct token
    astNode->children.push_back(exprA);    // Left child
    astNode->children.push_back(exprB);    // Right child
    
    // If you need to store a value/name in the node:
    // astNode->data = mode;  // For Id nodes, function names, etc.
    
    return astNode;
}
```

### Pattern Reference: Existing Nodes

#### Simple Value Node (Constant)

```cpp
Node* GraphCompiler::BuildConstant(const VisualNode& node) {
    // Just return the stored value, no inputs needed
    const std::string* val = GetField(node, "Value");
    if (!val) return MakeNumberNode(0.0);
    
    // Try parsing as number
    try { return MakeNumberNode(std::stod(*val)); }
    catch (...) {}
    
    // Check if boolean
    if (*val == "true") return MakeBoolNode(true);
    if (*val == "false") return MakeBoolNode(false);
    
    // Otherwise treat as string
    return MakeStringNode(*val);
}
```

#### Binary Operator (Operator, Comparison, Logic)

```cpp
Node* GraphCompiler::BuildOperator(const VisualNode& node) {
    // Named pin access (robust to pin reordering)
    const Pin* pinA = GetInputPinByName(node, "A");
    const Pin* pinB = GetInputPinByName(node, "B");
    if (!pinA || !pinB) {
        Error("Operator needs A and B inputs");
        return nullptr;
    }
    
    // Get operator from field, convert to Token
    const std::string* opStr = GetField(node, "Op");
    Token tok = opStr ? OperatorToken(*opStr) : Token::Add;
    
    // Create node with operator token
    Node* root = MakeNode(tok);  // Token::Add, Token::Sub, etc.
    
    // Recursively compile inputs
    Node* left = BuildExpr(*pinA);
    Node* right = BuildExpr(*pinB);
    if (HasError) { delete root; return nullptr; }
    
    // Add children in order: left, right
    root->children.push_back(left);
    root->children.push_back(right);
    
    return root;
}
```

#### Control Flow (Branch)

```cpp
Node* GraphCompiler::BuildBranch(const VisualNode& node) {
    // Get condition from input pin
    const Pin* condPin = GetInputPinByName(node, "Condition");
    if (!condPin) {
        Error("Branch needs Condition pin");
        return nullptr;
    }
    
    // Build the IF node
    Node* ifNode = MakeNode(Token::If);
    Node* condExpr = BuildExpr(*condPin);
    if (HasError) { delete ifNode; return nullptr; }
    
    // Children: [0]=condition, [1]=true body, [2]=else clause (optional)
    ifNode->children.push_back(condExpr);
    
    // Create empty scope for true branch
    Node* trueScope = MakeNode(Token::Scope);
    ifNode->children.push_back(trueScope);
    
    // Optional: add else branch
    Node* falseScope = MakeNode(Token::Scope);
    Node* elseNode = MakeNode(Token::Else);
    elseNode->children.push_back(falseScope);
    ifNode->children.push_back(elseNode);
    
    // TODO: Fill scopes with statements from connected flow pins
    return ifNode;
}
```

### Key Concepts

#### Named Pin Access (Not Positional)

**DO THIS** (robust):
```cpp
const Pin* input = GetInputPinByName(node, "Value");
```

**NOT THIS** (fragile):
```cpp
const Pin* input = &node.inPins[0];  // Breaks if pins reordered!
```

#### Recursive Compilation

`BuildExpr(pinId)` compiles the entire upstream subgraph connected to that pin. The compiler handles cycles and caching automatically.

#### Field vs Pin

- **Pin**: Connection point for data flow (compiled recursively)
- **Field**: User-editable value stored in the node (accessed directly)

Example: An `Operator` node has:
- **Pins**: `"A"` and `"B"` inputs (compiled from upstream nodes)
- **Field**: `"Op"` dropdown (stored string like `"add"`, `"sub"`)

### Testing Your Node

1. **Build**: Ensure no compile errors
2. **Create**: Drag node from palette in UI
3. **Connect**: Wire inputs from other nodes
4. **Compile**: Click compile button, check console for errors
5. **Verify AST**: Add debug output in your builder to print the AST structure

### Common Issues

| Problem | Cause | Solution |
|---------|-------|----------|
| Builder never called | Missing case in `BuildNode()` switch | Add your `NodeType` case |
| `GetInputPinByName` returns null | Pin name mismatch | Check descriptor pin name matches builder lookup |
| Crash on compile | Null pointer from `BuildExpr()` | Check if input pin is connected, handle nullptr |
| Wrong result | Field access incorrect | Print field name/value to debug |

### EMI AST Structure

EMI uses the `Node` type from `Parser/Node.h` to represent AST nodes. The AST is **not** object-oriented with different classes for each node type. Instead, it uses a **single Node struct** with a token type field.

**Key Files to Read:**
- `Parser/Node.h` - Defines the Node struct and its structure
- `Parser/Lexer.h` - Defines the Token enum (all possible node types)
- `Parser/Parser.h` - Shows how EMI parses text into AST (optional reference)

**Node Structure:**
```cpp
struct Node {
    Token type;                  // What kind of node this is (Add, Number, If, etc.)
    Variant data;                // Stores values: double, bool, string, etc.
    std::vector<Node*> children; // Child nodes (operands, body statements, etc.)
    
    void print(std::string indent); // Debug: print the tree structure
};
```

**Token Enum** (from `Parser/Lexer.h`):
Common tokens you'll use:
- **Literals**: `Token::Number`, `Token::True`, `Token::False`, `Token::Literal` (string), `Token::Null`
- **Identifiers**: `Token::Id` (variable/function names)
- **Arithmetic**: `Token::Add`, `Token::Sub`, `Token::Mult`, `Token::Div`
- **Comparison**: `Token::Equal`, `Token::NotEqual`, `Token::Less`, `Token::LessEqual`, `Token::Larger`, `Token::LargerEqual`
- **Logic**: `Token::And`, `Token::Or`, `Token::Not`
- **Control Flow**: `Token::If`, `Token::Else`, `Token::For`, `Token::While`
- **Variables**: `Token::VarDeclare`, `Token::Assign`, `Token::Increment`, `Token::Decrement`
- **Functions**: `Token::Definition`, `Token::Return`, `Token::FunctionCall`
- **Structure**: `Token::Scope` (code block), `Token::None` (error/empty)

---

## Understanding AST (Abstract Syntax Tree)

If you haven't worked with AST before, here's what you need to know:

### What is an AST?

An **Abstract Syntax Tree** is a tree representation of code. Each node represents a construct (like an operation, a value, or a control flow statement). The tree structure captures the relationships between these constructs.

**Example:** The expression `5 + 3 * 2` becomes this tree:

```
        Add
       /   \
      5    Mult
           /   \
          3     2
```

The tree shows that multiplication happens first (it's deeper in the tree), then addition.

### How Visual Nodes Become AST

Your visual node graph is essentially a flowchart. The compiler converts it to an AST that EMI can execute:

1. **Visual Graph** (what user sees):
```
[Constant: 5] ---> [Operator: +] ---> [Output]
[Constant: 3] --/
```

2. **AST** (what EMI executes):
```
Function "__graph__"
  └─ Return
       └─ Add
            ├─ Number(5)
            └─ Number(3)
```

### Node Construction Patterns

#### 1. Leaf Nodes (Values)

Leaf nodes have no children - they represent literal values or variable references:

```cpp
// Number literal: just the value
Node* num = MakeNode(Token::Number);
num->data = 42.0;

// Boolean: type tells you which boolean
Node* bool1 = MakeNode(Token::True);   // data ignored, type = True
Node* bool2 = MakeNode(Token::False);

// String literal
Node* str = MakeNode(Token::Literal);
str->data = "hello";

// Variable reference
Node* var = MakeNode(Token::Id);
var->data = "myVariable";  // The variable name
```

#### 2. Binary Operations (Two Inputs)

Binary operations have exactly 2 children: left and right operands:

```cpp
// Create: 5 + 3
Node* add = MakeNode(Token::Add);
add->children.push_back(MakeNumberNode(5.0));  // left
add->children.push_back(MakeNumberNode(3.0));  // right

// Same pattern for all binary ops:
// Arithmetic: Add, Sub, Mult, Div
// Comparison: Equal, NotEqual, Less, LessEqual, Larger, LargerEqual
// Logic: And, Or
```

**Children Order Matters:**
- `children[0]` = left operand
- `children[1]` = right operand

For `A - B`, you must put A first, B second (subtraction isn't commutative).

#### 3. Unary Operations (One Input)

Unary operations have 1 child:

```cpp
// Create: NOT condition
Node* notNode = MakeNode(Token::Not);
notNode->children.push_back(BuildExpr(conditionPin));  // The operand

// Also: Increment, Decrement
Node* incr = MakeNode(Token::Increment);
incr->children.push_back(MakeIdNode("counter"));
```

#### 4. Control Flow (Multiple Children)

Control flow nodes structure their children in specific orders:

**If Statement:**
```cpp
Node* ifNode = MakeNode(Token::If);
ifNode->children.push_back(conditionExpr);   // [0] = condition
ifNode->children.push_back(trueBodyScope);   // [1] = if-body

// Optional: Add else clause
Node* elseNode = MakeNode(Token::Else);
elseNode->children.push_back(falseBodyScope);
ifNode->children.push_back(elseNode);        // [2] = else clause
```

**For Loop:**
```cpp
Node* forNode = MakeNode(Token::For);
forNode->children.push_back(initStatement);  // [0] = var i = 0
forNode->children.push_back(conditionExpr);  // [1] = i < count
forNode->children.push_back(incrementExpr);  // [2] = i++
forNode->children.push_back(bodyScope);      // [3] = loop body
```

**Scope (Code Block):**
```cpp
Node* scope = MakeNode(Token::Scope);
scope->children.push_back(statement1);  // Add each statement
scope->children.push_back(statement2);
scope->children.push_back(statement3);
// Children can be in any order - they execute sequentially
```

#### 5. Assignments and Declarations

**Variable Declaration:**
```cpp
// var myVar = 10;
Node* decl = MakeNode(Token::VarDeclare);
decl->children.push_back(MakeIdNode("myVar"));    // [0] = name
decl->children.push_back(MakeNumberNode(10.0));   // [1] = initial value
```

**Assignment:**
```cpp
// myVar = 20;
Node* assign = MakeNode(Token::Assign);
assign->children.push_back(MakeIdNode("myVar"));  // [0] = target
assign->children.push_back(MakeNumberNode(20.0)); // [1] = new value
```

#### 6. Functions

**Function Definition:**
```cpp
// def myFunction() { body }
Node* funcDef = MakeNode(Token::Definition);
funcDef->children.push_back(MakeIdNode("myFunction")); // [0] = name
funcDef->children.push_back(bodyScope);                // [1] = body

// With parameters (TODO: check EMI docs for parameter syntax)
```

**Return Statement:**
```cpp
// return result;
Node* ret = MakeNode(Token::Return);
ret->children.push_back(resultExpr);  // [0] = return value
```

**Function Call:**
```cpp
// print(42);
Node* call = MakeNode(Token::FunctionCall);
call->data = "print";                      // Function name in data
call->children.push_back(MakeNumberNode(42.0)); // Arguments as children
```

### Real Example: Operator Node Compilation

Here's how `BuildOperator()` works in detail:

```cpp
Node* GraphCompiler::BuildOperator(const VisualNode& n) {
    // 1. Get visual node inputs by name
    const Pin* pinA = GetInputPinByName(n, "A");
    const Pin* pinB = GetInputPinByName(n, "B");
    
    // 2. Get operator type from field ("add", "sub", etc.)
    const std::string* opStr = GetField(n, "Op");
    Token tok = OperatorToken(*opStr);  // "add" -> Token::Add
    
    // 3. Create AST node with that token
    Node* root = MakeNode(tok);
    
    // 4. Recursively compile inputs (may trigger whole subgraph compilation!)
    Node* lhs = BuildExpr(*pinA);  // Compiles everything connected to pin A
    Node* rhs = BuildExpr(*pinB);  // Compiles everything connected to pin B
    
    // 5. Attach children in order
    root->children.push_back(lhs);   // Left operand
    root->children.push_back(rhs);   // Right operand
    
    return root;
}
```

**What BuildExpr Does:**
`BuildExpr(pin)` finds what's connected to that pin and compiles it:
- If connected to another node → recursively compile that node's output
- If not connected → return default value (0, false, "", or null)

This creates a **recursive tree** - each node compiles its inputs, which compile their inputs, etc.

### Debugging Your AST

EMI's `Node` has a `print()` method. Use it to visualize your AST:

```cpp
Node* ast = gc.Compile(state.GetNodes(), state.GetLinks());
ast->print("");  // Prints tree structure to console
```

Output looks like:
```
Scope
  Definition
    Id: __graph__
    Scope
      Return
        Add
          Number: 5
          Number: 3
```

This shows the structure clearly - you can see parent-child relationships and verify your tree is correct.

### Common AST Mistakes

| Mistake | Problem | Solution |
|---------|---------|----------|
| Wrong child order | Math wrong (e.g., 5-3 becomes 3-5) | Always put left operand first, right second |
| Forgetting to set `data` | Numbers/strings/IDs have no value | Set `node->data` for all leaf nodes |
| Wrong Token type | AST won't compile or does wrong operation | Use helper functions: `OperatorToken()`, `CompareToken()`, etc. |
| Memory leak | Created node but not added to tree | Every node must be returned or deleted |
| Null children | Crash during compilation | Check `BuildExpr()` returns non-null before adding |
| Wrong number of children | EMI parser confused | If expects 2 children (binary op), add exactly 2 |

### Further Reading

Once EMI is properly integrated (via CMake FetchContent), you can read:
- `build/_deps/emi-src/Parser/Node.h` - Full Node struct definition
- `build/_deps/emi-src/Parser/Lexer.h` - Complete Token enum
- `build/_deps/emi-src/Parser/Parser.h` - How EMI parses AST
- EMI documentation - Execution model, bytecode compilation

Until then, refer to [graphCompiler.cpp](src/core/compiler/graphCompiler.cpp) for working examples of every pattern.