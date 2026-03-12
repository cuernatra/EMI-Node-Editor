#include "pinResolver.h"

void PinResolver::Build(const std::vector<VisualNode>& nodes,
                        const std::vector<Link>&       links)
{
    inputToSource_.clear();
    nodeById_.clear();

    // Index all nodes by ID first.
    std::unordered_map<ed::PinId, PinSource> outputPinToSource;

    for (const VisualNode& n : nodes)
    {
        if (!n.alive) continue;
        nodeById_[n.id] = &n;

        for (int i = 0; i < static_cast<int>(n.outPins.size()); ++i)
        {
            outputPinToSource[n.outPins[i].id] = { &n, i };
        }
    }

    // For each live link, map endPinId (input) -> source node + pin index.
    for (const Link& lnk : links)
    {
        if (!lnk.alive) continue;

        auto srcIt = outputPinToSource.find(lnk.startPinId);
        if (srcIt != outputPinToSource.end())
        {
            inputToSource_[lnk.endPinId] = srcIt->second;
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
