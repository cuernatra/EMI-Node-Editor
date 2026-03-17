/**
 * @file pinResolver.h
 * @brief Pin connection resolution for recursive AST building
 * 
 * Pre-processes link lists into fast lookups mapping input pins to their
 * source (output pin on upstream node). GraphCompiler uses this to
 * recursively build AST subtrees: "what expression feeds this input pin?"
 * 
 * @author Jenny
 */

#pragma once
#include "../graph/visualNode.h"
#include "../graph/link.h"
#include "imgui_node_editor.h"
#include <unordered_map>
#include <cstdint>

namespace ed = ax::NodeEditor;

// Hash specialization for ed::PinId to use as unordered_map key
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

/**
 * @brief Identifies the source of a connected input pin
 * 
 * Represents where an input pin's value comes from:
 * - node: The upstream VisualNode with the output pin
 * - pinIdx: Index into node->outPins (which output pin on that node)
 * 
 * Used during recursive AST building to traverse from outputs back
 * through inputs, accumulating the expression tree.
 * 
 * @author Jenny
 */
struct PinSource
{
    const VisualNode* node    = nullptr;  ///< Source node (nullptr if unconnected)
    int               pinIdx  = 0;        ///< Index into node->outPins
};

/**
 * @brief Identifies where a flow output pin continues execution.
 */
struct FlowTarget
{
    const VisualNode* node   = nullptr;  ///< Downstream node receiving the flow signal
    int               pinIdx = 0;        ///< Index into node->inPins (the flow input that is connected)
};


/**
 * @brief Fast lookup for input pin → source mappings
 * 
 * Pre-processes the link list into a hash map for O(1) lookups during
 * recursive AST construction. GraphCompiler queries this to find:
 * "what AST subtree should I build for this input pin?"
 * 
 * Also provides node ID → VisualNode* lookups for convenience.
 * 
 * Usage:
 * @code
 * PinResolver resolver;
 * resolver.Build(nodes, links);
 * 
 * const PinSource* src = resolver.Resolve(inputPinId);
 * if (src && src->node) {
 *     // Found upstream node feeding this input
 *     const Pin& srcPin = src->node->outPins[src->pinIdx];
 * }
 * @endcode
 * 
 * @author Jenny 
 */
class PinResolver
{
public:
    /**
     * @brief Build resolver maps from graph data
     * @param nodes All nodes in the graph
     * @param links All connections between pins
     * 
     * Populates internal hash maps:
     * - inputToSource_: inputPinId → (sourceNode, sourcePinIndex)
     * - nodeById_: nodeId → VisualNode*
     * 
     * Call this once before calling Resolve() or FindNode().
     */
    void Build(const std::vector<VisualNode>& nodes,
               const std::vector<Link>&       links);

    /**
     * @brief Find the source of an input pin
     * @param inputPinId The input pin to look up
     * @return Pointer to PinSource, or nullptr if unconnected
     * 
     * Returns nullptr for disconnected inputs or invalid pin IDs.
     * The returned pointer is valid until the next Build() call.
     */
    const PinSource* Resolve(ed::PinId inputPinId) const;

    /**
     * @brief Find downstream target of a flow output pin.
     * @param flowOutputPinId Output flow pin ID
     * @return Pointer to FlowTarget, or nullptr if not connected
     */
    const FlowTarget* ResolveFlow(ed::PinId flowOutputPinId) const;

    /**
     * @brief Find a node by its ID
     * @param nodeId The node ID to look up
     * @return Pointer to VisualNode, or nullptr if not found
     * 
     * Convenience function for node lookups during AST traversal.
     * The returned pointer is valid until the next Build() call.
     */
    const VisualNode* FindNode(ed::NodeId nodeId) const;

private:
    // inputPinId -> PinSource
    std::unordered_map<ed::PinId, PinSource> inputToSource_;

    // nodeId -> VisualNode*
    std::unordered_map<ed::NodeId, const VisualNode*> nodeById_;

    // flow output pin ID -> downstream flow input target
    std::unordered_map<ed::PinId, FlowTarget> flowOutputToTarget_;
};
