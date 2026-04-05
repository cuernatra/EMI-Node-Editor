#include "nodeFactory.h"
#include "nodeRegistry.h"
#include "../graph/nodeField.h"
#include <cassert>
#include <iostream>

// Internal helpers.

// Fill node pins/fields from descriptor.
// IdSource returns the next pin id (new ids or loaded ids).
template<typename IdSource>
static void PopulateFromDescriptor(VisualNode& n, const NodeDescriptor& desc, IdSource pinIdSource)
{
    // Build pins from descriptor templates.
    for (const PinDescriptor& pd : desc.pins)
    {
        uint32_t pid = pinIdSource();
        Pin p = MakePin(pid, n.id, desc.type,
                        pd.name, pd.type, pd.isInput, pd.isMultiInput);

        // Keep input/output lists separate for editor usage.
        if (pd.isInput)
            n.inPins.push_back(p);
        else
            n.outPins.push_back(p);
    }

    // Build editable fields with default values.
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

    VisualNode n;
    n.id         = ed::NodeId(nodeId);
    n.nodeType   = type;
    n.title      = desc->label;
    n.initialPos = pos;
    // Loaded nodes are positioned later from saved initialPos.
    // Setting this true here can force bad default positions to be resaved.
    n.positioned = false;

    if (desc->deserialize)
    {
        if (!desc->deserialize(n, *desc, pinIds))
        {
            std::cerr << "CreateNodeFromTypeWithIds: deserialization failed\n";
            return {};
        }
    }
    else
    {
        if (pinIds.size() != desc->pins.size())
        {
            std::cerr << "CreateNodeFromTypeWithIds: Pin ID count mismatch\n";
            return {};
        }

        int pinIndex = 0;
        PopulateFromDescriptor(n, *desc, [&]() -> uint32_t {
            return static_cast<uint32_t>(pinIds[pinIndex++]);
        });
    }

    return n;
}
