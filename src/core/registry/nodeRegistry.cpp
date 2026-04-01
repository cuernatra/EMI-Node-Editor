#include "nodeRegistry.h"
#include "../compiler/graphCompiler.h"

// ---------------------------------------------------------------------------
// Registry population
// Add new node types here and nowhere else.
// ---------------------------------------------------------------------------

NodeRegistry::NodeRegistry()
{
    // ------------------------------------------------------------------
    // Start  —  Event entry output (UE-style white execution flow)
    // ------------------------------------------------------------------
    descriptors_[NodeType::Start] = {
        NodeType::Start,
        "Start",
        {
            { "Exec", PinType::Flow, /*isInput=*/false }
        },
        {},
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildStart(n); }
    };

    // ------------------------------------------------------------------
    // Constant  —  single editable Number output
    // ------------------------------------------------------------------
descriptors_[NodeType::Constant] = {
    NodeType::Constant,
    "Constant",
    {
        { "Value", PinType::Number, /*isInput=*/false }
    },
    {
        // Keep Type first so changing type updates Value widget/reset immediately.
        { "Type",  PinType::String, "Number" },
        { "Value", PinType::Number, "0.0" }
    },
    [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildConstant(n); }
};

    // ------------------------------------------------------------------
    // Operator  —  A op B -> Result  (operator chosen via field)
    // ------------------------------------------------------------------
    descriptors_[NodeType::Operator] = {
        NodeType::Operator,
        "Operator",
        {
            { "In",     PinType::Flow,   /*isInput=*/true  },
            { "A",      PinType::Number, /*isInput=*/true  },
            { "B",      PinType::Number, /*isInput=*/true  },
            { "Out",    PinType::Flow,   /*isInput=*/false },
            { "Result", PinType::Number, /*isInput=*/false }
        },
        {
            { "Op", PinType::String, "+" },  // Rendered as combo: + - * /
            { "A",  PinType::Number, "0.0" },
            { "B",  PinType::Number, "0.0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildOperator(n); }
    };

    // ------------------------------------------------------------------
    // Comparison  —  A cmp B -> Bool result
    // ------------------------------------------------------------------
    descriptors_[NodeType::Comparison] = {
        NodeType::Comparison,
        "Compare",
        {
            { "In",     PinType::Flow,    /*isInput=*/true  },
            { "A",      PinType::Number,  /*isInput=*/true  },
            { "B",      PinType::Number,  /*isInput=*/true  },
            { "Out",    PinType::Flow,    /*isInput=*/false },
            { "Result", PinType::Boolean, /*isInput=*/false }
        },
        {
            { "Op", PinType::String, ">=" },  // Rendered as combo: == != < <= > >=
            { "A",  PinType::Number, "0.0" },
            { "B",  PinType::Number, "0.0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildComparison(n); }
    };

    // ------------------------------------------------------------------
    // Logic  —  Boolean A logic B -> Bool result
    // ------------------------------------------------------------------
    descriptors_[NodeType::Logic] = {
        NodeType::Logic,
        "Logic",
        {
            { "In",     PinType::Flow,    /*isInput=*/true,  /*isMultiInput=*/false },
            { "A",      PinType::Boolean, /*isInput=*/true,  /*isMultiInput=*/false },
            { "B",      PinType::Boolean, /*isInput=*/true,  /*isMultiInput=*/false },
            { "Out",    PinType::Flow,    /*isInput=*/false, /*isMultiInput=*/false },
            { "Result", PinType::Boolean, /*isInput=*/false }
        },
        {
            { "Op", PinType::String, "AND" },  // Rendered as combo: AND OR
            { "A",  PinType::Boolean, "false" },
            { "B",  PinType::Boolean, "false" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildLogic(n); }
    };

    // ------------------------------------------------------------------
    // Not  —  Boolean invert A -> Result
    // ------------------------------------------------------------------
    descriptors_[NodeType::Not] = {
        NodeType::Not,
        "Not",
        {
            { "In",     PinType::Flow,    /*isInput=*/true  },
            { "A",      PinType::Boolean, /*isInput=*/true  },
            { "Out",    PinType::Flow,    /*isInput=*/false },
            { "Result", PinType::Boolean, /*isInput=*/false }
        },
        {
            { "A",  PinType::Boolean, "false" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildNot(n); }
    };

    // ------------------------------------------------------------------
    // Draw Rect — preview-only rectangle renderer
    // ------------------------------------------------------------------
    descriptors_[NodeType::DrawRect] = {
        NodeType::DrawRect,
        "Draw Rect",
        {
            { "In", PinType::Flow, /*isInput=*/true },
            { "X", PinType::Number, /*isInput=*/true },
            { "Y", PinType::Number, /*isInput=*/true },
            { "W", PinType::Number, /*isInput=*/true },
            { "H", PinType::Number, /*isInput=*/true },
            { "R", PinType::Number, /*isInput=*/true },
            { "G", PinType::Number, /*isInput=*/true },
            { "B", PinType::Number, /*isInput=*/true },
            { "Out", PinType::Flow, /*isInput=*/false }
        },
        {
            { "Color", PinType::String, "120, 180, 255" },
            { "X", PinType::Number, "0.0" },
            { "Y", PinType::Number, "0.0" },
            { "W", PinType::Number, "1.0" },
            { "H", PinType::Number, "1.0" },
            { "R", PinType::Number, "120.0" },
            { "G", PinType::Number, "180.0" },
            { "B", PinType::Number, "255.0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildDrawRect(n); }
    };

    // ------------------------------------------------------------------
    // Draw Grid — preview-only grid renderer that participates in flow order
    // ------------------------------------------------------------------
    descriptors_[NodeType::DrawGrid] = {
        NodeType::DrawGrid,
        "Draw Grid",
        {
            { "In", PinType::Flow, /*isInput=*/true },
            { "X", PinType::Number, /*isInput=*/true },
            { "Y", PinType::Number, /*isInput=*/true },
            { "W", PinType::Number, /*isInput=*/true },
            { "H", PinType::Number, /*isInput=*/true },
            { "R", PinType::Number, /*isInput=*/true },
            { "G", PinType::Number, /*isInput=*/true },
            { "B", PinType::Number, /*isInput=*/true },
            { "Out", PinType::Flow, /*isInput=*/false }
        },
        {
            { "Color", PinType::String, "30, 30, 38" },
            { "X", PinType::Number, "0.0" },
            { "Y", PinType::Number, "0.0" },
            { "W", PinType::Number, "40.0" },
            { "H", PinType::Number, "60.0" },
            { "R", PinType::Number, "30.0" },
            { "G", PinType::Number, "30.0" },
            { "B", PinType::Number, "38.0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildDrawGrid(n); }
    };

    // ------------------------------------------------------------------
    // Delay — flow-only ordering helper for preview/testing
    // ------------------------------------------------------------------
    descriptors_[NodeType::Delay] = {
        NodeType::Delay,
        "Delay",
        {
            { "In", PinType::Flow, /*isInput=*/true },
            { "Duration", PinType::Number, /*isInput=*/true },
            { "Out", PinType::Flow, /*isInput=*/false }
        },
        {
            { "Duration", PinType::Number, "1000.0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildDelay(n); }
    };

    // ------------------------------------------------------------------
    // Sequence  —  Flow in -> Then outputs (expandable)
    // ------------------------------------------------------------------
    descriptors_[NodeType::Sequence] = {
        NodeType::Sequence,
        "Sequence",
        {
            { "In",     PinType::Flow, /*isInput=*/true },
            { "Then 0", PinType::Flow, /*isInput=*/false }
        },
        {},
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildSequence(n); }
    };

    // ------------------------------------------------------------------
    // Branch  —  Flow in + Bool condition -> True / False flow out
    // ------------------------------------------------------------------
    descriptors_[NodeType::Branch] = {
        NodeType::Branch,
        "Branch",
        {
            { "In",        PinType::Flow,    /*isInput=*/true  },
            { "Condition", PinType::Boolean, /*isInput=*/true  },
            { "True",      PinType::Flow,    /*isInput=*/false },
            { "False",     PinType::Flow,    /*isInput=*/false }
        },
        {},
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildBranch(n); }
    };

    // ------------------------------------------------------------------
    // Loop  —  Flow in + count -> Body flow + Completed flow
    // ------------------------------------------------------------------
    descriptors_[NodeType::Loop] = {
        NodeType::Loop,
        "Loop",
        {
            { "In",        PinType::Flow,   /*isInput=*/true  },
            { "Start",     PinType::Number, /*isInput=*/true  },
            { "Count",     PinType::Number, /*isInput=*/true  },
            { "Body",      PinType::Flow,   /*isInput=*/false },
            { "Completed", PinType::Flow,   /*isInput=*/false },
            { "Index",     PinType::Number, /*isInput=*/false }
        },
        {
            { "Start", PinType::Number, "0" },
            { "Count", PinType::Number, "0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildLoop(n); }
    };

    // ------------------------------------------------------------------
    // For Each  —  Flow in + array -> Body flow + element output
    // ------------------------------------------------------------------
    descriptors_[NodeType::ForEach] = {
        NodeType::ForEach,
        "For Each",
        {
            { "In",        PinType::Flow,  /*isInput=*/true  },
            { "Array",     PinType::Array, /*isInput=*/true  },
            { "Body",      PinType::Flow,  /*isInput=*/false },
            { "Completed", PinType::Flow,  /*isInput=*/false },
            { "Element",   PinType::Any,   /*isInput=*/false }
        },
        {
            { "Element Type", PinType::String, "Any" },
            { "Array", PinType::Array, "[]" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildForEach(n); }
    };

    // ------------------------------------------------------------------
    // Array Get  —  array + index -> value
    // Reads one value from array by zero-based index.
    // ------------------------------------------------------------------
    descriptors_[NodeType::ArrayGetAt] = {
        NodeType::ArrayGetAt,
        "Array Get",
        {
            { "Array", PinType::Array,  /*isInput=*/true  },
            { "Index", PinType::Number, /*isInput=*/true  },
            { "Value", PinType::Any,    /*isInput=*/false }
        },
        {
            { "Array", PinType::Array, "[]" },
            { "Index", PinType::Number, "0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildArrayGetAt(n); }
    };

    // ------------------------------------------------------------------
    // Array Add  —  Flow in + array + index + value -> Flow out
    // Inserts Value at Index (UE5 style "Insert").
    // ------------------------------------------------------------------
    descriptors_[NodeType::ArrayAddAt] = {
        NodeType::ArrayAddAt,
        "Array Add",
        {
            { "In",    PinType::Flow,   /*isInput=*/true  },
            { "Array", PinType::Array,  /*isInput=*/true  },
            { "Index", PinType::Number, /*isInput=*/true  },
            { "Value", PinType::Any,    /*isInput=*/true  },
            { "Out",   PinType::Flow,   /*isInput=*/false }
        },
        {
            { "Array", PinType::Array, "[]" },
            { "Index", PinType::Number, "0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildArrayAddAt(n); }
    };

    // ------------------------------------------------------------------
    // Array Remove  —  Flow in + array + index -> Flow out
    // Removes item at Index.
    // ------------------------------------------------------------------
    descriptors_[NodeType::ArrayRemoveAt] = {
        NodeType::ArrayRemoveAt,
        "Array Remove",
        {
            { "In",    PinType::Flow,   /*isInput=*/true  },
            { "Array", PinType::Array,  /*isInput=*/true  },
            { "Index", PinType::Number, /*isInput=*/true  },
            { "Out",   PinType::Flow,   /*isInput=*/false }
        },
        {
            { "Array", PinType::Array, "[]" },
            { "Index", PinType::Number, "0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildArrayRemoveAt(n); }
    };

    // ------------------------------------------------------------------
    // Grid Node Schema  —  defines default struct-like layout for world cells
    // Slot layout:
    // [0]=Id, [1]=Tag, [2]=Type, [3]=X, [4]=Y, [5]=R, [6]=G, [7]=B,
    // [8]=IsWall, [9]=Distance, [10]=Neighbors
    // ------------------------------------------------------------------
    descriptors_[NodeType::GridNodeSchema] = {
        NodeType::GridNodeSchema,
        "Grid Node Schema",
        {
            { "Schema", PinType::Array, /*isInput=*/false }
        },
        {
            { "Id",        PinType::Number,  "0" },
            { "Tag",       PinType::String,  "node_0" },
            { "Type",      PinType::String,  "Empty" },
            { "X",         PinType::Number,  "0" },
            { "Y",         PinType::Number,  "0" },
            { "R",         PinType::Number,  "120" },
            { "G",         PinType::Number,  "180" },
            { "B",         PinType::Number,  "255" },
            { "IsWall",    PinType::Boolean, "false" },
            { "Distance",  PinType::Number,  "0" },
            { "Neighbors", PinType::Array,   "[]" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildGridNodeSchema(n); }
    };

    // ------------------------------------------------------------------
    // Grid Node Create  —  instantiate one node from schema/overrides
    // ------------------------------------------------------------------
    descriptors_[NodeType::GridNodeCreate] = {
        NodeType::GridNodeCreate,
        "Grid Node Create",
        {
            { "Schema",    PinType::Array,   /*isInput=*/true  },
            { "Id",        PinType::Number,  /*isInput=*/true  },
            { "Tag",       PinType::String,  /*isInput=*/true  },
            { "Type",      PinType::String,  /*isInput=*/true  },
            { "X",         PinType::Number,  /*isInput=*/true  },
            { "Y",         PinType::Number,  /*isInput=*/true  },
            { "R",         PinType::Number,  /*isInput=*/true  },
            { "G",         PinType::Number,  /*isInput=*/true  },
            { "B",         PinType::Number,  /*isInput=*/true  },
            { "IsWall",    PinType::Boolean, /*isInput=*/true  },
            { "Distance",  PinType::Number,  /*isInput=*/true  },
            { "Neighbors", PinType::Array,   /*isInput=*/true  },
            { "Node",      PinType::Array,   /*isInput=*/false }
        },
        {
            { "Schema",    PinType::Array,   "[]" },
            { "Id",        PinType::Number,  "0" },
            { "Tag",       PinType::String,  "node_0" },
            { "Type",      PinType::String,  "Empty" },
            { "X",         PinType::Number,  "0" },
            { "Y",         PinType::Number,  "0" },
            { "R",         PinType::Number,  "120" },
            { "G",         PinType::Number,  "180" },
            { "B",         PinType::Number,  "255" },
            { "IsWall",    PinType::Boolean, "false" },
            { "Distance",  PinType::Number,  "0" },
            { "Neighbors", PinType::Array,   "[]" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildGridNodeCreate(n); }
    };

    // ------------------------------------------------------------------
    // Grid Node Update  —  update one existing node (immutable-style return)
    // ------------------------------------------------------------------
    descriptors_[NodeType::GridNodeUpdate] = {
        NodeType::GridNodeUpdate,
        "Grid Node Update",
        {
            { "Node",      PinType::Array,   /*isInput=*/true  },
            { "Id",        PinType::Number,  /*isInput=*/true  },
            { "Tag",       PinType::String,  /*isInput=*/true  },
            { "Type",      PinType::String,  /*isInput=*/true  },
            { "X",         PinType::Number,  /*isInput=*/true  },
            { "Y",         PinType::Number,  /*isInput=*/true  },
            { "R",         PinType::Number,  /*isInput=*/true  },
            { "G",         PinType::Number,  /*isInput=*/true  },
            { "B",         PinType::Number,  /*isInput=*/true  },
            { "IsWall",    PinType::Boolean, /*isInput=*/true  },
            { "Distance",  PinType::Number,  /*isInput=*/true  },
            { "Neighbors", PinType::Array,   /*isInput=*/true  },
            { "Updated",   PinType::Array,   /*isInput=*/false }
        },
        {
            { "Node",      PinType::Array,   "[]" },
            { "Id",        PinType::Number,  "0" },
            { "Tag",       PinType::String,  "node_0" },
            { "Type",      PinType::String,  "Empty" },
            { "X",         PinType::Number,  "0" },
            { "Y",         PinType::Number,  "0" },
            { "R",         PinType::Number,  "120" },
            { "G",         PinType::Number,  "180" },
            { "B",         PinType::Number,  "255" },
            { "IsWall",    PinType::Boolean, "false" },
            { "Distance",  PinType::Number,  "0" },
            { "Neighbors", PinType::Array,   "[]" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildGridNodeUpdate(n); }
    };

    // ------------------------------------------------------------------
    // Grid Node Delete  —  remove node instance from array by index
    // ------------------------------------------------------------------
    descriptors_[NodeType::GridNodeDelete] = {
        NodeType::GridNodeDelete,
        "Grid Node Delete",
        {
            { "In",    PinType::Flow,   /*isInput=*/true  },
            { "Nodes", PinType::Array,  /*isInput=*/true  },
            { "Index", PinType::Number, /*isInput=*/true  },
            { "Out",   PinType::Flow,   /*isInput=*/false }
        },
        {
            { "Nodes", PinType::Array,  "[]" },
            { "Index", PinType::Number, "0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildGridNodeDelete(n); }
    };

    // ------------------------------------------------------------------
    // Struct Define  — named compact schema, custom fields stored in array text
    // Field format example: ["id:Number", "type:String", "neighbors:Array"]
    // ------------------------------------------------------------------
    descriptors_[NodeType::StructDefine] = {
        NodeType::StructDefine,
        "Struct Define",
        {
            { "Schema", PinType::Array, /*isInput=*/false }
        },
        {
            { "Struct Name", PinType::String, "test" },
            { "Fields", PinType::Array, "[\"id:Number\", \"type:String\"]" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildStructDefine(n); }
    };

    // ------------------------------------------------------------------
    // Struct Create  — dynamic input pins are generated from matching schema
    // ------------------------------------------------------------------
    descriptors_[NodeType::StructCreate] = {
        NodeType::StructCreate,
        "Struct Create",
        {
            { "Struct", PinType::String, /*isInput=*/true  },
            { "Value",  PinType::Any,    /*isInput=*/true  },
            { "Item",   PinType::Array,  /*isInput=*/false }
        },
        {
            { "Struct Name", PinType::String, "test" },
            { "Schema Fields", PinType::Array, "[]" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildStructCreate(n); }
    };

    // ------------------------------------------------------------------
    // Struct Get Field  — read one named field from struct instance
    // ------------------------------------------------------------------
    descriptors_[NodeType::StructGetField] = {
        NodeType::StructGetField,
        "Struct Get Field",
        {
            { "Item",  PinType::Array,  /*isInput=*/true  },
            { "Value", PinType::Any,    /*isInput=*/false }
        },
        {
            { "Struct Name", PinType::String, "test" },
            { "Field", PinType::String, "id" },
            { "Schema Fields", PinType::Array, "[]" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildStructGetField(n); }
    };

    // ------------------------------------------------------------------
    // Struct Set Field  — update one named field in struct instance
    // ------------------------------------------------------------------
    descriptors_[NodeType::StructSetField] = {
        NodeType::StructSetField,
        "Struct Set Field",
        {
            { "Item",  PinType::Array,  /*isInput=*/true  },
            { "Value", PinType::Any,    /*isInput=*/true  },
            { "Out",   PinType::Array,  /*isInput=*/false }
        },
        {
            { "Struct Name", PinType::String, "test" },
            { "Field", PinType::String, "id" },
            { "Schema Fields", PinType::Array, "[]" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildStructSetField(n); }
    };

    // ------------------------------------------------------------------
    // Struct Delete  — remove struct instance from array by id/index input
    // ------------------------------------------------------------------
    descriptors_[NodeType::StructDelete] = {
        NodeType::StructDelete,
        "Struct Delete",
        {
            { "In",    PinType::Flow,   /*isInput=*/true  },
            { "Array", PinType::Array,  /*isInput=*/true  },
            { "Id",    PinType::Number, /*isInput=*/true  },
            { "Out",   PinType::Flow,   /*isInput=*/false }
        },
        {
            { "Id", PinType::Number, "0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildStructDelete(n); }
    };

    // ------------------------------------------------------------------
    // While  —  Flow in + condition -> Body flow + Completed flow
    // ------------------------------------------------------------------
    descriptors_[NodeType::While] = {
        NodeType::While,
        "While",
        {
            { "In",        PinType::Flow,    /*isInput=*/true  },
            { "Condition", PinType::Boolean, /*isInput=*/true  },
            { "Body",      PinType::Flow,    /*isInput=*/false },
            { "Completed", PinType::Flow,    /*isInput=*/false }
        },
        {},
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildWhile(n); }
    };

    // ------------------------------------------------------------------
    // Variable  —  Named variable with get/set flow
    // ------------------------------------------------------------------
    descriptors_[NodeType::Variable] = {
        NodeType::Variable,
        "Set Variable",
        {
            { "In",    PinType::Flow, /*isInput=*/true  },
            { "Default", PinType::Any,  /*isInput=*/true  },
            { "Out",   PinType::Flow, /*isInput=*/false },
            { "Value", PinType::Any,  /*isInput=*/false }
        },
        {
            { "Variant", PinType::String, "Set" },
            { "Name",    PinType::String, "myVar" },
            { "Type",    PinType::String, "Number" },
            { "Default", PinType::String, "0.0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildVariable(n); }
    };

    // ------------------------------------------------------------------
    // Function  —  Call a named function
    // ------------------------------------------------------------------
    descriptors_[NodeType::Function] = {
        NodeType::Function,
        "Function",
        {
            { "In",     PinType::Flow, /*isInput=*/true,  /*isMultiInput=*/true },
            { "Out",    PinType::Flow, /*isInput=*/false }
        },
        {
            { "Name", PinType::String, "myFunction" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildFunction(n); }
    };

    // ------------------------------------------------------------------
    // Output  —  Flow-triggered debug print sink node
    // Display name in UI: "Debug Print"
    // ------------------------------------------------------------------
    descriptors_[NodeType::Output] = {
        NodeType::Output,
        "Debug Print",
        {
            { "In",    PinType::Flow, /*isInput=*/true,  /*isMultiInput=*/true },
            { "Value", PinType::Any,  /*isInput=*/true }
        },
        {
            { "Label", PinType::String, "result" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildOutput(n); }
    };
}

// ---------------------------------------------------------------------------

const NodeRegistry& NodeRegistry::Get()
{
    static NodeRegistry instance;
    return instance;
}

const NodeDescriptor* NodeRegistry::Find(NodeType type) const
{
    auto it = descriptors_.find(type);
    return it != descriptors_.end() ? &it->second : nullptr;
}

const std::unordered_map<NodeType, NodeDescriptor>& NodeRegistry::All() const
{
    return descriptors_;
}
