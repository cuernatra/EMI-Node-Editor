#include "nodeRegistry.h"
#include "../compiler/graphCompiler.h"
#include "../graph/visualNode.h"

namespace
{
bool PopulateExactPinsAndFields(VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds)
{
    if (pinIds.size() != desc.pins.size())
        return false;

    size_t pinIndex = 0;
    for (const PinDescriptor& pd : desc.pins)
    {
        const uint32_t pinId = static_cast<uint32_t>(pinIds[pinIndex++]);
        Pin p = MakePin(pinId, n.id, desc.type, pd.name, pd.type, pd.isInput, pd.isMultiInput);
        if (pd.isInput)
            n.inPins.push_back(p);
        else
            n.outPins.push_back(p);
    }

    for (const FieldDescriptor& fd : desc.fields)
        n.fields.push_back(MakeNodeField(fd));

    return true;
}
}

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
        // Dynamic value type (Number/Boolean/String/Array) is resolved from Type field.
        { "Value", PinType::Any, /*isInput=*/false }
    },
    {
        // Keep Type first so changing type updates Value widget/reset immediately.
        { "Type",  PinType::String, "Number" },
        { "Value", PinType::Any, "0.0" }
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
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildSequence(n); },
        [](VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds) -> bool {
            if (pinIds.size() < desc.pins.size())
                return false;

            if (!PopulateExactPinsAndFields(n, desc, std::vector<int>(pinIds.begin(), pinIds.begin() + desc.pins.size())))
                return false;

            for (size_t i = desc.pins.size(); i < pinIds.size(); ++i)
            {
                const int thenIndex = static_cast<int>(n.outPins.size());
                n.outPins.push_back(MakePin(
                    static_cast<uint32_t>(pinIds[i]),
                    n.id,
                    desc.type,
                    "Then " + std::to_string(thenIndex),
                    PinType::Flow,
                    false
                ));
            }

            return true;
        }
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
            { "Index", PinType::Number, "0" },
            { "Add Type", PinType::String, "Number" },
            { "Add Value", PinType::Any, "0.0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildArrayAddAt(n); }
    };

    // ------------------------------------------------------------------
    // Array Replace  —  Flow in + array + index + value -> Flow out
    // Replaces one item at Index (implemented as Remove + Insert sequence).
    // ------------------------------------------------------------------
    descriptors_[NodeType::ArrayReplaceAt] = {
        NodeType::ArrayReplaceAt,
        "Array Replace",
        {
            { "In",    PinType::Flow,   /*isInput=*/true  },
            { "Array", PinType::Array,  /*isInput=*/true  },
            { "Index", PinType::Number, /*isInput=*/true  },
            { "Value", PinType::Any,    /*isInput=*/true  },
            { "Out",   PinType::Flow,   /*isInput=*/false }
        },
        {
            { "Array", PinType::Array, "[]" },
            { "Index", PinType::Number, "0" },
            { "Replace Type", PinType::String, "Number" },
            { "Replace Value", PinType::Any, "0.0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildArrayReplaceAt(n); }
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
            { "Index", PinType::Number, "0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildArrayRemoveAt(n); }
    };

    // ------------------------------------------------------------------
    // Array Length  —  array -> item count (Number)
    // ------------------------------------------------------------------
    descriptors_[NodeType::ArrayLength] = {
        NodeType::ArrayLength,
        "Array Length",
        {
            { "Array",  PinType::Array,  /*isInput=*/true  },
            { "Length", PinType::Number, /*isInput=*/false }
        },
        {
            { "Array", PinType::Array, "[]" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildArrayLength(n); }
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
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildStructCreate(n); },
        [](VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds) -> bool {
            if (pinIds.size() == desc.pins.size())
                return PopulateExactPinsAndFields(n, desc, pinIds);

            // Struct Create has dynamic schema-driven input pins.
            // Keep all saved pin IDs so links can be restored, then layout refresh
            // will normalize names/types from loaded schema fields.
            if (pinIds.size() < 2)
                return false;

            n.inPins.push_back(MakePin(
                static_cast<uint32_t>(pinIds[0]),
                n.id,
                desc.type,
                "Struct",
                PinType::String,
                true
            ));

            for (size_t i = 1; i + 1 < pinIds.size(); ++i)
            {
                n.inPins.push_back(MakePin(
                    static_cast<uint32_t>(pinIds[i]),
                    n.id,
                    desc.type,
                    "Field " + std::to_string(i),
                    PinType::Any,
                    true
                ));
            }

            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(pinIds.back()),
                n.id,
                desc.type,
                "Item",
                PinType::Array,
                false
            ));

            for (const FieldDescriptor& fd : desc.fields)
                n.fields.push_back(MakeNodeField(fd));

            return true;
        }
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
            { "Out",   PinType::Flow, /*isInput=*/false }
        },
        {
            { "Variant", PinType::String, "Set" },
            { "Name",    PinType::String, "myVar" },
            { "Type",    PinType::String, "Number" },
            { "Default", PinType::String, "0.0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildVariable(n); },
        [](VisualNode& n, const NodeDescriptor& desc, const std::vector<int>& pinIds) -> bool {
            if (pinIds.size() == 1)
            {
                n.outPins.push_back(MakePin(
                    static_cast<uint32_t>(pinIds[0]),
                    n.id,
                    desc.type,
                    "Value",
                    PinType::Any,
                    false
                ));

                for (const FieldDescriptor& fd : desc.fields)
                    n.fields.push_back(MakeNodeField(fd));

                for (NodeField& f : n.fields)
                {
                    if (f.name == "Variant")
                    {
                        f.value = "Get";
                        break;
                    }
                }
                return true;
            }

            return PopulateExactPinsAndFields(n, desc, pinIds);
        }
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

    auto setCategory = [&](NodeType type, const char* category)
    {
        auto it = descriptors_.find(type);
        if (it != descriptors_.end())
            it->second.category = category;
    };

    setCategory(NodeType::Start, "Events");

    setCategory(NodeType::Constant, "Data");
    setCategory(NodeType::Variable, "Data");
    setCategory(NodeType::ArrayGetAt, "Data");
    setCategory(NodeType::ArrayLength, "Data");

    setCategory(NodeType::StructDefine, "Structs");
    setCategory(NodeType::StructCreate, "Structs");

    setCategory(NodeType::Operator, "Logic");
    setCategory(NodeType::Comparison, "Logic");
    setCategory(NodeType::Logic, "Logic");
    setCategory(NodeType::Not, "Logic");
    setCategory(NodeType::DrawGrid, "Logic");
    setCategory(NodeType::Function, "Logic");

    setCategory(NodeType::Delay, "Flow");
    setCategory(NodeType::DrawRect, "Flow");
    setCategory(NodeType::Sequence, "Flow");
    setCategory(NodeType::Branch, "Flow");
    setCategory(NodeType::Loop, "Flow");
    setCategory(NodeType::ForEach, "Flow");
    setCategory(NodeType::ArrayAddAt, "Flow");
    setCategory(NodeType::ArrayRemoveAt, "Flow");
    setCategory(NodeType::While, "Flow");
    setCategory(NodeType::Output, "Flow");

    auto variableIt = descriptors_.find(NodeType::Variable);
    if (variableIt != descriptors_.end())
    {
        variableIt->second.paletteVariants = {
            { "Set Variable", "Variable:Set" },
            { "Get Variable", "Variable:Get" }
        };
    }
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
