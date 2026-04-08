#pragma once
#include "../graph/visualNode.h"
#include "../graph/link.h"
#include "imgui_node_editor.h"
#include <unordered_map>
#include <cstdint>

namespace ed = ax::NodeEditor;

// Allow NodeId/PinId as unordered_map keys.
namespace std {
template<>
struct hash<ed::PinId>
{
    size_t operator()(const ed::PinId& id) const noexcept
    {
        return hash<uintptr_t>()(id.Get());
    }
};

template<>
struct hash<ed::NodeId>
{
    size_t operator()(const ed::NodeId& id) const noexcept
    {
        return hash<uintptr_t>()(id.Get());
    }
};
}

/** Where an input pin gets its value from. */
struct PinSource
{
    const VisualNode* node   = nullptr;  ///< Upstream node.
    int               pinIdx = 0;        ///< Index in node->outPins.
};

/** Where a flow output wire leads next. */
struct FlowTarget
{
    const VisualNode* node   = nullptr;  ///< Downstream node.
    int               pinIdx = 0;        ///< Index in node->inPins.
};

/**
 * Builds fast lookup tables from a graph snapshot and answers two questions:
 * "what feeds this input pin?" and "where does this flow wire go?".
 */
class PinResolver
{
public:
    /** Rebuild lookup tables from the current nodes and links. */
    void Build(const std::vector<VisualNode>& nodes,
               const std::vector<Link>&       links);

    /** Returns the source of an input pin, or nullptr if disconnected. */
    const PinSource* Resolve(ed::PinId inputPinId) const;

    /** Returns the target of a flow output pin, or nullptr if disconnected. */
    const FlowTarget* ResolveFlow(ed::PinId flowOutputPinId) const;

    /** Find a node by id, or nullptr if not found. */
    const VisualNode* FindNode(ed::NodeId nodeId) const;

private:
    std::unordered_map<ed::PinId,  PinSource>              inputToSource_;
    std::unordered_map<ed::NodeId, const VisualNode*>      nodeById_;
    std::unordered_map<ed::PinId,  FlowTarget>             flowOutputToTarget_;
};
