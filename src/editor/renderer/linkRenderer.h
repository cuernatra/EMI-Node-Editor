#pragma once

#include "../../core/graphModel/link.h"

#include <vector>

/**
 * @brief Draw all live links on the node canvas.
 *
 * Call inside ed::Begin()/ed::End().
 */
void DrawLinks(const std::vector<Link>& links);
