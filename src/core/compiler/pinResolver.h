/** @file pinResolver.h */
/** @brief Fast lookup tables for pin connections used by compiler code. */

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

namespace ed = ax::NodeEditor;

/** @brief Source info for one connected input pin. */
struct PinSource
{
    const VisualNode* node    = nullptr;  ///< Upstream node.
    int               pinIdx  = 0;        ///< Index in node->outPins.
};

/** @brief Target info for one connected flow output pin. */
struct FlowTarget
{
    const VisualNode* node   = nullptr;  ///< Downstream node.
    int               pinIdx = 0;        ///< Index in node->inPins.
};


/** @brief Builds and serves pin lookup maps for compile passes. */
class PinResolver
{
public:
    /** @brief Rebuild lookup tables from current nodes and links. */
    void Build(const std::vector<VisualNode>& nodes,
               const std::vector<Link>&       links);

    /** @brief Resolve input pin source, or nullptr if disconnected. */
    const PinSource* Resolve(ed::PinId inputPinId) const;

    /** @brief Resolve flow output target, or nullptr if disconnected. */
    const FlowTarget* ResolveFlow(ed::PinId flowOutputPinId) const;

    /** @brief Find node by id, or nullptr if missing. */
    const VisualNode* FindNode(ed::NodeId nodeId) const;

private:
    // input pin -> source output info
    std::unordered_map<ed::PinId, PinSource> inputToSource_;

    // node id -> node pointer
    std::unordered_map<ed::NodeId, const VisualNode*> nodeById_;

    // flow output pin -> downstream flow input target
    std::unordered_map<ed::PinId, FlowTarget> flowOutputToTarget_;
};
