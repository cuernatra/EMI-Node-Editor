#ifndef IDGEN_H
#define IDGEN_H

#include "imgui_node_editor.h"

namespace ed = ax::NodeEditor;

/**
 * @brief Simple ID generator for graph elements
 * 
 * Generates unique sequential IDs for nodes, pins, and links in the visual graph editor.
 * Uses a single shared counter to ensure all IDs are unique across different element types.
 * 
 * @note IDs start at 1 and increment monotonically. ID 0 is reserved as invalid.
 * 
 */
struct IdGen
{
    /// Next available ID value (starts at 1)
    int next = 1;

    /**
     * @brief Set the next ID value to be generated
     * @param v The value to set as the next ID
     * 
     * Useful for deserializing graphs where you need to ensure new IDs
     * don't conflict with existing ones.
     */
    void SetNext(int v) { next = v; }

    /**
     * @brief Generate a new unique node ID
     * @return A unique NodeId for use in the node editor
     */
    ed::NodeId NewNode() { return ed::NodeId(next++); }
    
    /**
     * @brief Generate a new unique pin ID
     * @return A unique PinId for use in the node editor
     */
    ed::PinId  NewPin()  { return ed::PinId(next++); }
    
    /**
     * @brief Generate a new unique link ID
     * @return A unique LinkId for use in the node editor
     */
    ed::LinkId NewLink() { return ed::LinkId(next++); }
};

#endif // IDGEN_H
