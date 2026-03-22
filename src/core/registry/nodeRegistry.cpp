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
            { "Op", PinType::String, "+" }   // Rendered as combo: + - * /
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
            { "Op", PinType::String, ">=" }  // Rendered as combo: == != < <= > >=
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
            { "Op", PinType::String, "AND" }  // Rendered as combo: AND OR NOT XOR
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildLogic(n); }
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
            { "Count",     PinType::Number, /*isInput=*/true  },
            { "Body",      PinType::Flow,   /*isInput=*/false },
            { "Completed", PinType::Flow,   /*isInput=*/false },
            { "Index",     PinType::Number, /*isInput=*/false }
        },
        {
            { "Start", PinType::Number, "0" }
        },
        [](GraphCompiler* compiler, const VisualNode& n) { return compiler->BuildLoop(n); }
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
