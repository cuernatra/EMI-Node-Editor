#include "nodeFactory.h"
#include "nodeRegistry.h"
#include "fieldWidget.h"
#include <cassert>
#include <iostream>

// Internal helpers

// Populate node's pins and fields from descriptor.
// IdSource is a callable that returns uint32 (supports new ID generation or loading from disk).
template<typename IdSource>
static void PopulateFromDescriptor(VisualNode& n, const NodeDescriptor& desc, IdSource pinIdSource)
{
    // Create pins from descriptor templates (unique ID, parent ID, metadata).
    for (const PinDescriptor& pd : desc.pins)
    {
        uint32_t pid = pinIdSource();  // Get next available ID (NEW or from array)
        Pin p = MakePin(pid, n.id, desc.type,
                        pd.name, pd.type, pd.isInput, pd.isMultiInput);

        // Separate inputs and outputs (editor needs them split for rendering).
        if (pd.isInput)
            n.inPins.push_back(p);
        else
            n.outPins.push_back(p);
    }

    // Create fields from descriptor templates (editable values with defaults).
    for (const FieldDescriptor& fd : desc.fields)
        n.fields.push_back(MakeNodeField(fd));
}



VisualNode CreateNodeFromType(NodeType type, IdGen& gen, ImVec2 pos)
{
    const NodeDescriptor* desc = NodeRegistry::Get().Find(type);
    assert(desc && "NodeType not registered");

    VisualNode n;
    n.id         = gen.NewNode();
    n.nodeType   = type;
    n.title      = desc->label;
    n.initialPos = pos;

    PopulateFromDescriptor(n, *desc, [&]() -> uint32_t {
        return static_cast<uint32_t>(gen.NewPin().Get());
    });

    return n;
}

// ---------------------------------------------------------------------------
// CreateNodeFromTypeWithIds
// ---------------------------------------------------------------------------

VisualNode CreateNodeFromTypeWithIds(NodeType type,
                                     int nodeId,
                                     const std::vector<int>& pinIds,
                                     ImVec2 pos)
{
    const NodeDescriptor* desc = NodeRegistry::Get().Find(type);
    if (!desc)
    {
        std::cerr << "CreateNodeFromTypeWithIds: NodeType not registered\n";
        return {};
    }

    const bool isSequence    = (type == NodeType::Sequence);
    const bool isVariable    = (type == NodeType::Variable);
    const bool isFunctionCall = (type == NodeType::FunctionCall);
    if ((!isSequence && !isFunctionCall && pinIds.size() != desc->pins.size()) ||
        ((isSequence || isFunctionCall) && pinIds.size() < desc->pins.size()))
    {
        std::cerr << "CreateNodeFromTypeWithIds: Pin ID count mismatch\n";
        return {};
    }

    assert((isSequence || isVariable || isFunctionCall || pinIds.size() == desc->pins.size()) && "Pin ID count mismatch");

    VisualNode n;
    n.id         = ed::NodeId(nodeId);
    n.nodeType   = type;
    n.title      = desc->label;
    n.initialPos = pos;
    n.positioned = true;

    int pinIndex = 0;
    PopulateFromDescriptor(n, *desc, [&]() -> uint32_t {
        return static_cast<uint32_t>(pinIds[pinIndex++]);
    });

    if (isSequence)
    {
        for (; pinIndex < static_cast<int>(pinIds.size()); ++pinIndex)
        {
            const int thenIndex = static_cast<int>(n.outPins.size());
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(pinIds[pinIndex]),
                n.id,
                n.nodeType,
                "Then " + std::to_string(thenIndex),
                PinType::Flow,
                false
            ));
        }
    }

    // FunctionCall: layout is [In, Arg0..ArgN, Out, Result]
    // The descriptor only defines In/Out/Result (3 pins).
    // Extra saved pins beyond the 3 base are Arg0..ArgN inserted before Out.
    if (type == NodeType::FunctionCall && pinIds.size() > 3)
    {
        // Rebuild: pop Out and Result from outPins, insert Arg pins, then re-add Out/Result.
        // The base PopulateFromDescriptor already created In(in), Out(out), Result(out).
        // Find the Out and Result pins.
        Pin outPin    = n.outPins[0];  // "Out"   (Flow)
        Pin resultPin = n.outPins[1];  // "Result" (Any)

        // Replace their IDs with the saved ones (pinIndex is at 3 after base populate).
        // Base populate consumed pinIds[0] (In), pinIds[1] (Out), pinIds[2] (Result).
        // Extra pinIds are for Arg0, Arg1, ...
        // Re-assign Out and Result IDs from the END of pinIds.
        const int totalPins    = static_cast<int>(pinIds.size());
        const int argCount     = totalPins - 3;                   // pins beyond In/Out/Result
        const int outPinIdx    = 1 + argCount;                    // Out is after In + all args
        const int resultPinIdx = 2 + argCount;                    // Result follows Out

        // Fix IDs that were wrongly assigned during base populate.
        // Base assigned: n.inPins[0].id = pinIds[0], n.outPins[0].id = pinIds[1], n.outPins[1].id = pinIds[2].
        // Correct layout: In=pinIds[0], Arg0=pinIds[1]..Arg{N-1}=pinIds[N], Out=pinIds[N+1], Result=pinIds[N+2].

        // Reset outPins so we can repopulate in the right order.
        n.outPins.clear();

        // Fix Out pin id.
        outPin.id = ed::PinId(static_cast<uint32_t>(pinIds[outPinIdx]));
        // Fix Result pin id.
        resultPin.id = ed::PinId(static_cast<uint32_t>(pinIds[resultPinIdx]));

        // Fix the In pin id (already correct – pinIds[0]).

        // Insert Arg pins into inPins (after In at index 0).
        for (int a = 0; a < argCount; ++a)
        {
            n.inPins.push_back(MakePin(
                static_cast<uint32_t>(pinIds[1 + a]),
                n.id,
                n.nodeType,
                "Arg" + std::to_string(a),
                PinType::Any,
                true
            ));
        }

        n.outPins.push_back(outPin);
        n.outPins.push_back(resultPin);
    }

    return n;
}
