#pragma once

#include "core/graphModel/types.h"

/**
 * @brief Drag payload used to spawn nodes from the palette.
 *
 * Sent through ImGui drag-and-drop ("SPAWN_NODE") to the graph canvas.
 */
struct NodeSpawnPayload
{
    char    title[32] = {};           ///< Node title/token (for example "For Each" or "Variable:Set").
    PinType pinType   = PinType::Any; ///< Source pin type from drag origin (used for auto-wiring).
};
