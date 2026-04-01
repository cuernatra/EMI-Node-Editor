#pragma once

/**
 * Helper functions for graph editor logic.
 *
 * This file keeps small shared rules and utility code out of GraphEditor.
 * Put things here when they are about graph rules or shared helper work,
 * not direct canvas drawing.
 */
#include "graphState.h"
#include "../core/graph/visualNode.h"
#include "imgui.h"
#include <string>
#include <vector>

namespace GraphEditorUtils
{
ImVec2 SnapToNodeGrid(const ImVec2& pos);
void ParseSpawnPayloadTitle(const char* payloadTitle, NodeType& outType, std::string& outVariableVariant);

NodeField* FindField(std::vector<NodeField>& fields, const char* name);
const NodeField* FindField(const std::vector<NodeField>& fields, const char* name);
Pin* FindPinByName(std::vector<Pin>& pins, const char* name);

bool DrawInspectorField(NodeField& field);
void DrawInspectorReadOnlyField(const NodeField& field);

bool RefreshVariableNodeTypes(GraphState& state);
bool RefreshOutputNodeInputTypes(GraphState& state);
bool RefreshLoopNodeLayout(GraphState& state);
bool RefreshForEachNodeLayout(GraphState& state);
bool RefreshDrawRectNodeLayout(GraphState& state);
bool RefreshStructNodeLayouts(GraphState& state);
bool SyncLinkTypesAndPruneInvalid(GraphState& state);
void DisconnectNonAnyLinksForPins(GraphState& state, const std::vector<ed::PinId>& pinIds);
}