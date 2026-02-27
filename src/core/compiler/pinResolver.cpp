#include "pinResolver.h"

void PinResolver::Build(const std::vector<VisualNode>& nodes,
                        const std::vector<Link>&       links)
{
    // Index all nodes by ID first.
    for (const VisualNode& n : nodes)
    {
        if (!n.alive) continue;
        nodeById_[n.id] = &n;
    }

    // For each live link, map endPinId (input) -> source node + pin index.
    for (const Link& lnk : links)
    {
        if (!lnk.alive) continue;

        // Find the node that owns the output pin.
        for (const VisualNode& n : nodes)
        {
            if (!n.alive) continue;
            for (int i = 0; i < (int)n.outPins.size(); ++i)
            {
                if (n.outPins[i].id == lnk.startPinId)
                {
                    inputToSource_[lnk.endPinId] = { &n, i };
                    break;
                }
            }
        }
    }
}

const PinSource* PinResolver::Resolve(ed::PinId inputPinId) const
{
    auto it = inputToSource_.find(inputPinId);
    return it != inputToSource_.end() ? &it->second : nullptr;
}

const VisualNode* PinResolver::FindNode(ed::NodeId nodeId) const
{
    auto it = nodeById_.find(nodeId);
    return it != nodeById_.end() ? it->second : nullptr;
}
