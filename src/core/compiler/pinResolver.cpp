#include "pinResolver.h"

void PinResolver::Build(const std::vector<VisualNode>& nodes,
                        const std::vector<Link>&       links)
{
    inputToSource_.clear();
    nodeById_.clear();
    flowOutputToTarget_.clear();

    // First pass: index alive nodes and pins.
    std::unordered_map<ed::PinId, PinSource> outputPinToSource;
    std::unordered_map<ed::PinId, FlowTarget> flowInputPinToTarget;

    for (const VisualNode& n : nodes)
    {
        if (!n.alive) continue;
        nodeById_[n.id] = &n;

        for (int i = 0; i < static_cast<int>(n.outPins.size()); ++i)
        {
            outputPinToSource[n.outPins[i].id] = { &n, i };
        }

        for (int i = 0; i < static_cast<int>(n.inPins.size()); ++i)
        {
            const Pin& inPin = n.inPins[i];
            if (inPin.type == PinType::Flow)
                flowInputPinToTarget[inPin.id] = { &n, i };
        }
    }

    // Second pass: use links to connect input pins to their source outputs.
    for (const Link& lnk : links)
    {
        if (!lnk.alive) continue;

        auto srcIt = outputPinToSource.find(lnk.startPinId);
        if (srcIt != outputPinToSource.end())
        {
            inputToSource_[lnk.endPinId] = srcIt->second;

            // Also track flow-to-flow wiring for execution order lookup.
            const PinSource& source = srcIt->second;
            if (source.node
                && source.pinIdx >= 0
                && source.pinIdx < static_cast<int>(source.node->outPins.size()))
            {
                const Pin& sourceOutPin = source.node->outPins[source.pinIdx];
                if (sourceOutPin.type == PinType::Flow)
                {
                    auto targetIt = flowInputPinToTarget.find(lnk.endPinId);
                    if (targetIt != flowInputPinToTarget.end())
                        flowOutputToTarget_[lnk.startPinId] = targetIt->second;
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

const FlowTarget* PinResolver::ResolveFlow(ed::PinId flowOutputPinId) const
{
    auto it = flowOutputToTarget_.find(flowOutputPinId);
    return it != flowOutputToTarget_.end() ? &it->second : nullptr;
}

const VisualNode* PinResolver::FindNode(ed::NodeId nodeId) const
{
    auto it = nodeById_.find(nodeId);
    return it != nodeById_.end() ? it->second : nullptr;
}
