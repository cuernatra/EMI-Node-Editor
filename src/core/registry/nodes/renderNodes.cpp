#include "../nodeRegistry.h"
#include "nodeCompileHelpers.h"

// Render nodes: typed wrappers around the drawcell / cleargrid / rendergrid
// native functions.  DrawRect and DrawGrid (in eventNodes.cpp) are separate
// preview-only nodes that do NOT emit runtime calls; these nodes do.

namespace
{
Node* CompileDrawCellNode(GraphCompiler* compiler, const VisualNode& n)
{
    auto build = [&](const char* name) -> Node*
    {
        const Pin* pin = FindInputPin(n, name);
        if (!pin) return MakeNumberLiteral(0.0);
        return BuildNumberOperand(compiler, n, *pin);
    };

    Node* x = build("X");
    Node* y = build("Y");
    Node* r = build("R");
    Node* g = build("G");
    Node* b = build("B");

    if (compiler->HasError) { delete x; delete y; delete r; delete g; delete b; return nullptr; }

    return compiler->EmitFunctionCall("drawcell", { x, y, r, g, b });
}

Node* CompileClearGridNode(GraphCompiler* compiler, const VisualNode& n)
{
    const Pin* wPin = FindInputPin(n, "W");
    const Pin* hPin = FindInputPin(n, "H");

    Node* w = wPin ? BuildNumberOperand(compiler, n, *wPin) : MakeNumberLiteral(20.0);
    Node* h = hPin ? BuildNumberOperand(compiler, n, *hPin) : MakeNumberLiteral(20.0);

    if (compiler->HasError) { delete w; delete h; return nullptr; }

    return compiler->EmitFunctionCall("cleargrid", { w, h });
}

Node* CompileRenderGridNode(GraphCompiler* compiler, const VisualNode&)
{
    return compiler->EmitFunctionCall("rendergrid", {});
}
}

void NodeRegistry::RegisterRenderNodes()
{
    Register({
        NodeType::DrawCell,
        "Draw Cell",
        {
            { "In", PinType::Flow,   true  },
            { "X",  PinType::Number, true  },
            { "Y",  PinType::Number, true  },
            { "R",  PinType::Number, true  },
            { "G",  PinType::Number, true  },
            { "B",  PinType::Number, true  },
            { "Out", PinType::Flow,  false }
        },
        {
            { "X", PinType::Number, "0.0" },
            { "Y", PinType::Number, "0.0" },
            { "R", PinType::Number, "220.0" },
            { "G", PinType::Number, "50.0"  },
            { "B", PinType::Number, "50.0"  }
        },
        CompileDrawCellNode,
        nullptr,
        "Render",
        {},
        "DrawCell",
        { "X", "Y" },
        NodeRenderStyle::Draw
    });

    Register({
        NodeType::ClearGrid,
        "Clear Grid",
        {
            { "In",  PinType::Flow,   true  },
            { "W",   PinType::Number, true  },
            { "H",   PinType::Number, true  },
            { "Out", PinType::Flow,   false }
        },
        {
            { "W", PinType::Number, "20.0" },
            { "H", PinType::Number, "20.0" }
        },
        CompileClearGridNode,
        nullptr,
        "Render",
        {},
        "ClearGrid",
        { "W", "H" },
        NodeRenderStyle::Draw
    });

    Register({
        NodeType::RenderGrid,
        "Render Grid",
        {
            { "In",  PinType::Flow, true  },
            { "Out", PinType::Flow, false }
        },
        {},
        CompileRenderGridNode,
        nullptr,
        "Render",
        {},
        "RenderGrid",
        {},
        NodeRenderStyle::Draw
    });
}
