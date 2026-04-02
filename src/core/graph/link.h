#ifndef LINK_H
#define LINK_H

#include "imgui_node_editor.h"
#include "types.h"
#include <vector>

namespace ed = ax::NodeEditor;

// ---------------------------------------------------------------------------
// Link
// ---------------------------------------------------------------------------

/**
 * @brief Represents a connection (edge) between two pins in the node graph
 * 
 * A Link connects an output pin to an input pin, with a specific data type
 * that determines the visual appearance of the connection wire. Links can be
 * marked as inactive (alive=false) for deletion.
 */
struct Link
{
    /// Unique identifier for this link in the node editor
    ed::LinkId id{};
    
    /// Source pin (output) of the connection
    ed::PinId  startPinId{};
    
    /// Destination pin (input) of the connection
    ed::PinId  endPinId{};
    
    /// Data type flowing through this link (used for color-coding)
    PinType    type = PinType::Any;
    
    /// Whether this link is active (false marks it for deletion)
    bool alive = true;
};

// ---------------------------------------------------------------------------
// Link utilities
// ---------------------------------------------------------------------------

/**
 * @brief Check if adding a link would create a cycle in the graph
 * @param links Current set of active links in the graph
 * @param startPin Output pin ID of the proposed new link
 * @param endPin Input pin ID of the proposed new link
 * @return true if the new link would create a cycle, false otherwise
 * 
 * Uses depth-first search to detect cycles. A cycle would create an invalid
 * graph where data dependencies form a loop, which cannot be evaluated.
 * 
 * @note This function is called during link creation to prevent invalid connections
 */
bool WouldCreateCycle(const std::vector<Link>& links,
                      ed::PinId startPin, ed::PinId endPin);

#endif // LINK_H
