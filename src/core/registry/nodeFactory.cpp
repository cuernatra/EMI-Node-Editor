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

    const bool isSequence = (type == NodeType::Sequence);
    const bool isVariable = (type == NodeType::Variable);
    const bool isLoop = (type == NodeType::Loop);
    const bool isDrawGrid = (type == NodeType::DrawGrid);
    const bool isStructCreate = (type == NodeType::StructCreate);
    if ((!isSequence && !isLoop && pinIds.size() != desc->pins.size()) ||
        (isSequence && pinIds.size() < desc->pins.size()))
    {
        // Backward compatibility for older Variable node saves
        // (legacy Set/Get layouts used different pin counts).
        if (!isVariable
            && !isStructCreate
            && !(isDrawGrid && pinIds.size() == desc->pins.size() + 1))
        {
            std::cerr << "CreateNodeFromTypeWithIds: Pin ID count mismatch\n";
            return {};
        }
    }

    assert((isSequence || isVariable || isLoop || isStructCreate || (isDrawGrid && pinIds.size() == desc->pins.size() + 1) || pinIds.size() == desc->pins.size()) && "Pin ID count mismatch");

    VisualNode n;
    n.id         = ed::NodeId(nodeId);
    n.nodeType   = type;
    n.title      = desc->label;
    n.initialPos = pos;
    n.positioned = true;

    int pinIndex = 0;

    if (isVariable && pinIds.size() != desc->pins.size())
    {
        // Legacy Variable layouts:
        // 1 pin  -> Get (Value out)
        // 2 pins -> Set (Set in, Value out)
        // 3 pins -> transitional (In, Set, Value)
        if (!pinIds.empty())
        {
            if (pinIds.size() == 1)
            {
                n.outPins.push_back(MakePin(
                    static_cast<uint32_t>(pinIds[0]), n.id, n.nodeType,
                    "Value", PinType::Any, false));
            }
            else if (pinIds.size() == 2)
            {
                n.inPins.push_back(MakePin(
                    static_cast<uint32_t>(pinIds[0]), n.id, n.nodeType,
                    "Set", PinType::Any, true));
                n.outPins.push_back(MakePin(
                    static_cast<uint32_t>(pinIds[1]), n.id, n.nodeType,
                    "Value", PinType::Any, false));
            }
            else
            {
                n.inPins.push_back(MakePin(
                    static_cast<uint32_t>(pinIds[0]), n.id, n.nodeType,
                    "In", PinType::Flow, true));
                n.inPins.push_back(MakePin(
                    static_cast<uint32_t>(pinIds[1]), n.id, n.nodeType,
                    "Set", PinType::Any, true));
                n.outPins.push_back(MakePin(
                    static_cast<uint32_t>(pinIds[2]), n.id, n.nodeType,
                    "Value", PinType::Any, false));
            }
        }

        // Legacy path still needs descriptor fields.
        for (const FieldDescriptor& fd : desc->fields)
            n.fields.push_back(MakeNodeField(fd));

        // Infer Variant from legacy pin layout.
        // 1 pin => old Get (Value output only), otherwise Set.
        for (NodeField& f : n.fields)
        {
            if (f.name == "Variant")
            {
                f.value = (pinIds.size() == 1) ? "Get" : "Set";
                break;
            }
        }
    }
    else if (isLoop && pinIds.size() != desc->pins.size())
    {
        // Legacy Loop layouts:
        // 4 pins -> In, Count, Body, Completed
        // 5 pins -> In, Count, Body, Completed, Index
        // Current  -> In, Start, Count, Body, Completed, Index
        if (pinIds.size() >= 1)
        {
            n.inPins.push_back(MakePin(
                static_cast<uint32_t>(pinIds[0]), n.id, n.nodeType,
                "In", PinType::Flow, true));
        }
        if (pinIds.size() >= 2)
        {
            n.inPins.push_back(MakePin(
                static_cast<uint32_t>(pinIds[1]), n.id, n.nodeType,
                "Count", PinType::Number, true));
        }
        if (pinIds.size() >= 3)
        {
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(pinIds[2]), n.id, n.nodeType,
                "Body", PinType::Flow, false));
        }
        if (pinIds.size() >= 4)
        {
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(pinIds[3]), n.id, n.nodeType,
                "Completed", PinType::Flow, false));
        }
        if (pinIds.size() >= 5)
        {
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(pinIds[4]), n.id, n.nodeType,
                "Index", PinType::Number, false));
        }

        for (const FieldDescriptor& fd : desc->fields)
            n.fields.push_back(MakeNodeField(fd));
    }
    else if (isDrawGrid && pinIds.size() == desc->pins.size() + 1)
    {
        // Legacy Draw Grid layout had an extra Step input pin between H and RGB.
        static const int kLegacyPinMap[] = { 0, 1, 2, 3, 4, 6, 7, 8, 9 };
        for (int mappedIndex : kLegacyPinMap)
        {
            const PinDescriptor& pd = desc->pins[pinIndex];
            Pin p = MakePin(static_cast<uint32_t>(pinIds[mappedIndex]), n.id, desc->type,
                            pd.name, pd.type, pd.isInput, pd.isMultiInput);
            if (pd.isInput)
                n.inPins.push_back(p);
            else
                n.outPins.push_back(p);
            ++pinIndex;
        }

        for (const FieldDescriptor& fd : desc->fields)
            n.fields.push_back(MakeNodeField(fd));
    }
    else if (isStructCreate && pinIds.size() != desc->pins.size())
    {
        // Struct Create has dynamic schema-driven input pins.
        // Keep all saved pin IDs so links can be restored, then layout refresh
        // will normalize names/types from loaded schema fields.
        if (!pinIds.empty())
        {
            n.inPins.push_back(MakePin(
                static_cast<uint32_t>(pinIds[0]),
                n.id,
                n.nodeType,
                "Struct",
                PinType::String,
                true
            ));

            for (size_t i = 1; i + 1 < pinIds.size(); ++i)
            {
                n.inPins.push_back(MakePin(
                    static_cast<uint32_t>(pinIds[i]),
                    n.id,
                    n.nodeType,
                    "Field " + std::to_string(i),
                    PinType::Any,
                    true
                ));
            }

            if (pinIds.size() >= 2)
            {
                n.outPins.push_back(MakePin(
                    static_cast<uint32_t>(pinIds.back()),
                    n.id,
                    n.nodeType,
                    "Item",
                    PinType::Array,
                    false
                ));
            }
        }

        for (const FieldDescriptor& fd : desc->fields)
            n.fields.push_back(MakeNodeField(fd));
    }
    else
    {
        PopulateFromDescriptor(n, *desc, [&]() -> uint32_t {
            return static_cast<uint32_t>(pinIds[pinIndex++]);
        });
    }

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

    return n;
}
