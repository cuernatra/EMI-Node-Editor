/**
 * @file nodeFactory.h
 * @brief Node instance creation from registry descriptors
 * 
 * Provides factory functions for creating VisualNode instances from their
 * descriptors. Handles both fresh node creation (with new IDs) and
 * deserialization (with existing IDs).
 * 
 */

#ifndef NODE_FACTORY_H
#define NODE_FACTORY_H

#include "../graph/visualNode.h"
#include "../graph/idGen.h"

// ---------------------------------------------------------------------------
// NodeFactory
// ---------------------------------------------------------------------------

/**
 * @brief Create a new node with freshly generated IDs
 * @param type The type of node to create
 * @param gen ID generator for creating unique IDs
 * @param pos Initial position on the canvas
 * @return Fully initialized VisualNode
 * 
 * Looks up the descriptor for the given NodeType, creates all pins and
 * fields as specified, and assigns fresh IDs from the generator.
 * 
 * Used when spawning new nodes from the palette or context menu.
 */
VisualNode CreateNodeFromType(NodeType type, IdGen& gen, ImVec2 pos);

/**
 * @brief Create a node with specific predefined IDs (deserialization)
 * @param type The type of node to create
 * @param nodeId The node ID to assign
 * @param pinIds Pin IDs in descriptor order (all inputs, then all outputs)
 * @param pos Initial position on the canvas
 * @return Fully initialized VisualNode
 * 
 * Used when loading nodes from disk where IDs are already known.
 * The pinIds vector must match the descriptor's pin order exactly:
 * first all input pins, then all output pins.
 * 
 * @warning pinIds.size() must match the total pin count in the descriptor
 */
VisualNode CreateNodeFromTypeWithIds(NodeType type,
                                     int nodeId,
                                     const std::vector<int>& pinIds,
                                     ImVec2 pos);

#endif // NODE_FACTORY_H
