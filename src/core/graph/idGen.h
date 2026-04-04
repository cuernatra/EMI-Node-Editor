#ifndef IDGEN_H
#define IDGEN_H

#include "imgui_node_editor.h"

namespace ed = ax::NodeEditor;

/** @brief Small counter that gives unique ids for nodes, pins, and links. */
struct IdGen
{
    /// Next id to return.
    int next = 1;

    /** @brief Set next id (used after loading saved graphs). */
    void SetNext(int v) { next = v; }

    /** @brief Create new node id. */
    ed::NodeId NewNode() { return ed::NodeId(next++); }
    
    /** @brief Create new pin id. */
    ed::PinId  NewPin()  { return ed::PinId(next++); }
    
    /** @brief Create new link id. */
    ed::LinkId NewLink() { return ed::LinkId(next++); }
};

#endif // IDGEN_H
