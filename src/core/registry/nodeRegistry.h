/**
 * @file nodeRegistry.h
 * @brief Central registry of all node type definitions
 * 
 * Singleton registry that maps NodeType enums to their descriptors.
 * All supported node types are registered here with their pin/field
 * configurations.
 * 
 */

#ifndef NODE_REGISTRY_H
#define NODE_REGISTRY_H

#include "nodeDescriptor.h"
#include <unordered_map>

// ---------------------------------------------------------------------------
// NodeRegistry
// ---------------------------------------------------------------------------

/**
 * @brief Singleton registry for node type definitions
 * 
 * Central repository mapping NodeType → NodeDescriptor. All node types
 * available in the editor are registered here with their complete
 * specifications (pins, fields, labels).
 * 
 * The registry is populated once on first access and remains immutable
 * thereafter. Node implementations live in nodeRegistry.cpp.
 * 
 * Usage:
 * @code
 * const NodeDescriptor* desc = NodeRegistry::Get().Find(NodeType::Operator);
 * if (desc) {
 *     // Use descriptor to create or render node
 * }
 * @endcode
 * 
 */
class NodeRegistry
{
public:
    /**
     * @brief Get the global registry instance
     * @return Reference to the singleton registry
     * 
     * Thread-safe initialization on first call.
     */
    static const NodeRegistry& Get();

    /**
     * @brief Look up a node type descriptor
     * @param type The NodeType to find
     * @return Pointer to descriptor, or nullptr if not registered
     * 
     * Returns nullptr for NodeType::Unknown or unregistered types.
     */
    const NodeDescriptor* Find(NodeType type) const;

    /**
     * @brief Get all registered descriptors
     * @return Map of all NodeType → NodeDescriptor mappings
     * 
     * Useful for building spawn menus or enumerating available node types.
     */
    const std::unordered_map<NodeType, NodeDescriptor>& All() const;

private:
    /**
     * @brief Private constructor - populates registry
     * 
     * Called once on first Get() access. Registers all supported node types.
     */
    NodeRegistry();

    /// Register one descriptor into the immutable map.
    void Register(NodeDescriptor descriptor);

    /// Category-specific registration helpers (implemented in nodes/*.cpp).
    void RegisterEventNodes();
    void RegisterDataNodes();
    void RegisterLogicNodes();
    void RegisterFlowNodes();
    void RegisterStructNodes();

    std::unordered_map<NodeType, NodeDescriptor> descriptors_;  ///< NodeType → descriptor map
};

#endif // NODE_REGISTRY_H
