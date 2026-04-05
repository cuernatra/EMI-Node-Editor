/** @file nodeRegistry.h */
/** @brief Global map of node types to their descriptors. */

#ifndef NODE_REGISTRY_H
#define NODE_REGISTRY_H

#include "nodeDescriptor.h"
#include <unordered_map>

/** @brief Singleton descriptor registry used by editor, loader, and compiler. */
class NodeRegistry
{
public:
    /** @brief Get global registry instance. */
    static const NodeRegistry& Get();

    /** @brief Find descriptor by node type, or nullptr. */
    const NodeDescriptor* Find(NodeType type) const;

    /** @brief Resolve save/spawn token to node type. */
    NodeType FindByToken(const std::string& token) const;

    /** @brief Access all registered descriptors. */
    const std::unordered_map<NodeType, NodeDescriptor>& All() const;

private:
    /** @brief Build registry content once. */
    NodeRegistry();

    /// Register one descriptor.
    void Register(NodeDescriptor descriptor);

    /// Category-specific registration helpers.
    void RegisterEventNodes();
    void RegisterDataNodes();
    void RegisterLogicNodes();
    void RegisterFlowNodes();
    void RegisterStructNodes();
    void RegisterDemoNodes();

    std::unordered_map<NodeType, NodeDescriptor> descriptors_;  ///< NodeType to descriptor map.
};

#endif // NODE_REGISTRY_H
