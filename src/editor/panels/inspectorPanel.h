/**
 * @brief Inspector panel UI for editing selected node fields
 *
 * This module owns the ImGui rendering and editing rules for the inspector.
 * The caller is responsible for selecting the node and setting the node-editor
 * context if required.
 */

#ifndef INSPECTORPANEL_H
#define INSPECTORPANEL_H

#include <cstdint>

class GraphState;

/**
 * @brief Inspector panel for viewing and editing the selected node
 *
 * Displays editable fields and metadata for the currently selected node.
 * The panel reads from and writes to graph state through the inspector UI.
 */

class InspectorPanel
{
public:
	/// Render inspector contents for the given selected node id.
	/// @param state Graph state to read/mutate
	/// @param selectedNodeRawId Raw node id value (as returned by node editor)
	void draw(GraphState& state, uintptr_t selectedNodeRawId);
};

#endif
