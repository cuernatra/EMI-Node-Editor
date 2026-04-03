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

    // Validate pin count: supportsVariablePins allows flexible counts, others require exact match
    const bool pinCountValid = desc->supportsVariablePins
        ? (pinIds.size() >= desc->pins.size())
        : (pinIds.size() == desc->pins.size());

    if (!pinCountValid)
    {
        std::cerr << "CreateNodeFromTypeWithIds: Pin ID count mismatch\n";
        return {};
    }

    assert(pinCountValid && "Pin ID count mismatch");

    VisualNode n;
    n.id         = ed::NodeId(nodeId);
    n.nodeType   = type;
    n.title      = desc->label;
    n.initialPos = pos;
    n.positioned = true;

    int pinIndex = 0;

    // Special handling for Variable nodes with single pin (Get variant)
    const bool isVariable = (type == NodeType::Variable);
    const bool isSequence = (type == NodeType::Sequence);
    const bool isStructCreate = (type == NodeType::StructCreate);

    if (isVariable && pinIds.size() == 1)
    {
        // Variable Get variant is intentionally a reduced layout (Value out only).
        n.outPins.push_back(MakePin(
            static_cast<uint32_t>(pinIds[0]), n.id, n.nodeType,
            "Value", PinType::Any, false));

        // Still needs descriptor fields.
        for (const FieldDescriptor& fd : desc->fields)
            n.fields.push_back(MakeNodeField(fd));

        // Force Variant to Get.
        for (NodeField& f : n.fields)
        {
            if (f.name == "Variant")
            {
                f.value = "Get";
                break;
            }
        }
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