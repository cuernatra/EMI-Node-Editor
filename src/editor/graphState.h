/**
 * @file graphState.h
 * @brief Central graph data store and state management
 * 
 * Maintains the complete graph state: nodes, links, ID generation,
 * and compilation status. Provides CRUD operations for graph elements
 * and accessors for various subsystems.
 * 
 * @author Atte Perkiö
 */

#pragma once

#include "../core/graph/visualNode.h"
#include "../core/graph/link.h"
#include "../core/graph/idGen.h"
#include <vector>
#include <string>

namespace ed = ax::NodeEditor;

/**
 * @brief Central container for all graph state
 * 
 * Encapsulates the complete visual graph:
 * - Collection of VisualNodes with their pins and fields
 * - Collection of Links connecting pins
 * - ID generator for creating unique element IDs
 * - Compilation status and messages
 * 
 * Provides a clean interface for editor operations while keeping
 * the internal data structure private.
 * 
 * @author Atte Perkiö
 */
class GraphState
{
public:
    /**
     * @brief Initialize empty graph state
     */
    GraphState();

    // Graph data access
    /**
     * @brief Get mutable reference to all nodes
     * @return Vector of all nodes in the graph
     */
    std::vector<VisualNode>& GetNodes() { return m_nodes; }
    
    /**
     * @brief Get mutable reference to all links
     * @return Vector of all links in the graph
     */
    std::vector<Link>& GetLinks() { return m_links; }
    
    /**
     * @brief Get const reference to all nodes
     * @return Vector of all nodes in the graph
     */
    const std::vector<VisualNode>& GetNodes() const { return m_nodes; }
    
    /**
     * @brief Get const reference to all links
     * @return Vector of all links in the graph
     */
    const std::vector<Link>& GetLinks() const { return m_links; }

    /**
     * @brief Clear all nodes and links from the graph
     * 
     * Removes all graph elements. Compilation status is preserved.
     */
    void Clear();

    // Node management
    /**
     * @brief Add a new node to the graph
     * @param node The node to add (copied)
     */
    void AddNode(const VisualNode& node);
    
    /**
     * @brief Delete a node and all its connected links
     * @param nodeId The ID of the node to delete
     */
    void DeleteNode(ed::NodeId nodeId);

    /**
     * @brief Find a pin by ID
     * @param pinId The pin ID to look up
     * @return Pointer to pin, or nullptr if not found
     */
    const Pin* FindPin(ed::PinId pinId) const;
    
    /**
     * @brief Remove all links connected to a node's pins
     * @param n The node whose links should be removed
     */
    void RemoveLinksForNode(const VisualNode& n);

    // Link management
    /**
     * @brief Add a new link to the graph
     * @param link The link to add (copied)
     */
    void AddLink(const Link& link);
    
    /**
     * @brief Delete a link from the graph
     * @param linkId The ID of the link to delete
     */
    void DeleteLink(ed::LinkId linkId);

    /**
     * @brief Mark graph state as changed
     */
    void MarkDirty() { m_dirty = true; }

    /**
     * @brief Check if graph state has unsaved changes
     */
    bool IsDirty() const { return m_dirty; }

    /**
     * @brief Clear dirty flag after saving
     */
    void ClearDirty() { m_dirty = false; }

    // Compile status
    /**
     * @brief Check if last compilation succeeded
     * @return true if last compilation was successful
     */
    bool IsCompileSuccess() const { return m_compileSuccess; }
    
    /**
     * @brief Get the last compilation message
     * @return Success result or error message from last compilation
     */
    const std::string& GetCompileMessage() const { return m_compileMessage; }
    
    /**
     * @brief Update compilation status and message
     * @param success Whether compilation succeeded
     * @param message Result or error message to display
     */
    void SetCompileStatus(bool success, const std::string& message);

    /**
     * @brief Get the ID generator
     * @return Reference to the ID generator for creating new elements
     */
    IdGen& GetIdGen() { return m_idGen; }

    /**
     * @brief Check if the graph contains an Output node
     * @return true if at least one Output node exists
     * 
     * Required for compilation - the Output node defines the graph's result.
     */
    bool HasOutputNode() const;

private:
    std::vector<VisualNode> m_nodes;           ///< All nodes in the graph
    std::vector<Link> m_links;                 ///< All links in the graph
    bool m_compileSuccess = false;             ///< Last compilation result
    std::string m_compileMessage;              ///< Last compilation message
    IdGen m_idGen;
    bool m_dirty = false;                       ///< Tracks unsaved changes
};
