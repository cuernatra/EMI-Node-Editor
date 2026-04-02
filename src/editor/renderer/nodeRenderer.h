#pragma once

#include "core/graph/idGen.h"
#include "core/graph/link.h"
#include "core/graph/visualNode.h"

#include <vector>

/** @brief Returns width for one field row (label + spacing + widget). */
float MeasureFieldWidth(const NodeField& field);

/** @brief Returns minimum content width needed to render node body. */
float MeasureNodeContentWidth(const VisualNode& n);

/** @brief Draws one pin row and icon with width-aware alignment. */
void DrawPin(const Pin& pin, float contentWidth, const std::vector<Link>* allLinks);

bool DrawVisualNode(VisualNode& n);
bool DrawVisualNode(VisualNode& n, IdGen* idGen);
bool DrawVisualNode(VisualNode& n, IdGen* idGen, const std::vector<VisualNode>* allNodes);
bool DrawVisualNode(VisualNode& n, IdGen* idGen, const std::vector<VisualNode>* allNodes, const std::vector<Link>* allLinks);
