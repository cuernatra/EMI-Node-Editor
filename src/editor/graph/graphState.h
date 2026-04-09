/** @file graphState.h */
/** @brief Owns nodes, links, ids, and compile status for the graph editor. */

#pragma once

#include "core/graphModel/visualNode.h"
#include "core/graphModel/link.h"
#include "core/graphModel/idGen.h"
#include <vector>
#include <string>

namespace ed = ax::NodeEditor;

/** @brief Mutable container for editor graph data and status flags. */
class GraphState
{
public:
    /** @brief Construct empty graph state. */
    GraphState();

    // Graph data access
    /** @brief Access mutable node storage. */
    std::vector<VisualNode>& GetNodes() { return m_nodes; }
    
    /** @brief Access mutable link storage. */
    std::vector<Link>& GetLinks() { return m_links; }
    
    /** @brief Access read-only node storage. */
    const std::vector<VisualNode>& GetNodes() const { return m_nodes; }
    
    /** @brief Access read-only link storage. */
    const std::vector<Link>& GetLinks() const { return m_links; }

    /** @brief Remove all nodes and links and reset compile status. */
    void Clear();

    // Node management
    /** @brief Add a node. */
    void AddNode(const VisualNode& node);
    
    /** @brief Soft-delete node and remove its links. */
    void DeleteNode(ed::NodeId nodeId);

    /** @brief Find pin by id, or nullptr. */
    const Pin* FindPin(ed::PinId pinId) const;
    
    /** @brief Remove links touching any pin on the given node. */
    void RemoveLinksForNode(const VisualNode& n);

    // Link management
    /** @brief Add a link. */
    void AddLink(const Link& link);
    
    /** @brief Delete a link by id. */
    void DeleteLink(ed::LinkId linkId);

    /** @brief Mark graph as modified. */
    void MarkDirty() { m_dirty = true; }

    /** @brief Return true when graph has unsaved changes. */
    bool IsDirty() const { return m_dirty; }

    /** @brief Clear modified flag. */
    void ClearDirty() { m_dirty = false; }

    // Compile status
    /** @brief Return last compile success flag. */
    bool IsCompileSuccess() const { return m_compileSuccess; }
    
    /** @brief Return latest compile result message. */
    const std::string& GetCompileMessage() const { return m_compileMessage; }
    
    /** @brief Set compile status and message. */
    void SetCompileStatus(bool success, const std::string& message);

    /** @brief Access id generator. */
    IdGen& GetIdGen() { return m_idGen; }

    /** @brief Return true when at least one Output node exists. */
    bool HasOutputNode() const;

    /** @brief Return true when graph has an alive node of the given type. */
    bool HasNodeType(NodeType type) const;

    /** @brief Return true when graph has any alive nodes. */
    bool HasAliveNodes() const;

private:
    std::vector<VisualNode> m_nodes;           ///< All nodes in the graph
    std::vector<Link> m_links;                 ///< All links in the graph
    bool m_compileSuccess = false;             ///< Last compilation result
    std::string m_compileMessage;              ///< Last compilation message
    IdGen m_idGen;
    bool m_dirty = false;                       ///< Tracks unsaved changes
};
