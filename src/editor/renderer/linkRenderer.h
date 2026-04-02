#pragma once

#include "../../core/graph/link.h"

#include <vector>

/**
 * @brief Render all active links in the node editor.
 *
 * Must be called within an ed::Begin() / ed::End() block.
 * Only draws links marked as alive=true.
 */
void DrawLinks(const std::vector<Link>& links);
