#pragma once

#include "core/graph/types.h"

/**
 * @brief Drag-and-drop payload for spawning new nodes from the palette.
 *
 * Passed via ImGui drag-and-drop ("SPAWN_NODE") from the node palette to the
 * graph editor canvas.
 */
struct NodeSpawnPayload
{
    char    title[32] = {};           ///< Node type name (e.g. "For Each" or "Variable:Set")
    PinType pinType   = PinType::Any; ///< Pin type that initiated the drag (for auto-wiring)
};
