/** @file nodeFactory.h */
/** @brief Build runtime VisualNode instances from registry descriptors. */

#ifndef NODE_FACTORY_H
#define NODE_FACTORY_H

#include "../graphModel/visualNode.h"
#include "../graphModel/idGen.h"

/** @brief Create a new node using fresh ids. */
VisualNode CreateNodeFromType(NodeType type, IdGen& gen, ImVec2 pos);

/** @brief Create a node from known ids (used while loading saved graphs). */
VisualNode CreateNodeFromTypeWithIds(NodeType type,
                                     int nodeId,
                                     const std::vector<int>& pinIds,
                                     ImVec2 pos);

#endif // NODE_FACTORY_H
