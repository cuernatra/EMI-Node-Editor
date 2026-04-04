#ifndef LINK_H
#define LINK_H

#include "imgui_node_editor.h"
#include "types.h"
#include <vector>

namespace ed = ax::NodeEditor;

// ---------------------------------------------------------------------------
// Link
// ---------------------------------------------------------------------------

/** @brief One wire between an output pin and an input pin. */
struct Link
{
    /// Unique link id in node editor.
    ed::LinkId id{};
    
    /// Output/source pin id.
    ed::PinId  startPinId{};
    
    /// Input/destination pin id.
    ed::PinId  endPinId{};
    
    /// Data type carried by this link.
    PinType    type = PinType::Any;
    
    /// False means logically deleted.
    bool alive = true;
};

// ---------------------------------------------------------------------------
// Link utilities
// ---------------------------------------------------------------------------

/**
 * @brief Return true if a new link would make a cycle.
 *
 * Used during link creation to block invalid self-referencing chains.
 */
bool WouldCreateCycle(const std::vector<Link>& links,
                      ed::PinId startPin, ed::PinId endPin);

#endif // LINK_H
