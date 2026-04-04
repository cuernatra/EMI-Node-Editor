#pragma once

#include "core/graph/idGen.h"
#include "core/graph/link.h"
#include "core/graph/visualNode.h"

#include <vector>

namespace NodeRendererSpecialCases
{
struct FieldRenderContext;

// Custom field rendering hooks.
// Add a new Handle* function and chain it in HandleCustomFieldRendering.
// Return true when the field was handled, false to fall back to generic draw.

bool HandleVariableField  (NodeField& field, FieldRenderContext& context);
bool HandleFlowStyleField (NodeField& field, FieldRenderContext& context);
bool HandleArrayStyleField(NodeField& field, FieldRenderContext& context);
bool HandleDrawStyleField (NodeField& field, FieldRenderContext& context);
bool HandleStructStyleField(NodeField& field, FieldRenderContext& context);

// Dispatcher that runs all Handle* rules in order.
bool HandleCustomFieldRendering(NodeField& field, FieldRenderContext& context);
}

// Public node draw entry points.
// Layout helpers stay in nodeRenderer.cpp.

bool DrawVisualNode(VisualNode& n);
bool DrawVisualNode(VisualNode& n, IdGen* idGen);
bool DrawVisualNode(VisualNode& n, IdGen* idGen, const std::vector<VisualNode>* allNodes);
bool DrawVisualNode(VisualNode& n, IdGen* idGen, const std::vector<VisualNode>* allNodes, const std::vector<Link>* allLinks);
