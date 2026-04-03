#pragma once

#include "core/graph/idGen.h"
#include "core/graph/link.h"
#include "core/graph/visualNode.h"

#include <vector>

namespace NodeRendererSpecialCases
{
struct FieldRenderContext;

// ─────────────────────────────────────────────────────────────────────────────
// HOW TO ADD CUSTOM FIELD RENDERING
//
// 1. Write a new Handle* function (declaration here, definition in .cpp):
//      bool HandleMyThing(NodeField& field, FieldRenderContext& context);
//    Return true  → your handler drew the field (skips the default draw).
//    Return false → fall through to the next handler / default draw.
//
// 2. Chain it in HandleCustomFieldRendering (nodeRenderer.cpp).
//
// What FieldRenderContext gives you:
//   context.node          – the VisualNode being drawn
//   context.allNodes      – all nodes in the graph (may be null)
//   context.flags         – booleans identifying node type/render style
//   context.changed       – set true if you modify a field value
//   context.deferred      – tracks which deferred input pins were drawn inline
//
//   context.drawFieldWithConnectionRule(field, pinName)
//       draws the field widget; switches to read-only when the pin is linked
//   context.drawDeferredPinAndField(field, pinName, &drewFlag)
//       draws an inline input-pin icon followed by the field widget
//   context.isInputPinConnected(pinName)
//       returns true when that input pin has an active link
// ─────────────────────────────────────────────────────────────────────────────

bool HandleVariableField  (NodeField& field, FieldRenderContext& context);
bool HandleFlowStyleField (NodeField& field, FieldRenderContext& context);
bool HandleArrayStyleField(NodeField& field, FieldRenderContext& context);
bool HandleDrawStyleField (NodeField& field, FieldRenderContext& context);
bool HandleStructStyleField(NodeField& field, FieldRenderContext& context);

// Main dispatcher — chains all Handle* functions above in order.
bool HandleCustomFieldRendering(NodeField& field, FieldRenderContext& context);
}

// Public node rendering entry points.
// Internal layout/drawing helpers are intentionally kept in nodeRenderer.cpp.

bool DrawVisualNode(VisualNode& n);
bool DrawVisualNode(VisualNode& n, IdGen* idGen);
bool DrawVisualNode(VisualNode& n, IdGen* idGen, const std::vector<VisualNode>* allNodes);
bool DrawVisualNode(VisualNode& n, IdGen* idGen, const std::vector<VisualNode>* allNodes, const std::vector<Link>* allLinks);
