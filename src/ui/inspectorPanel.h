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

class InspectorPanel
{
public:
	/// Render inspector contents for the given selected node id.
	/// @param state Graph state to read/mutate
	/// @param selectedNodeRawId Raw node id value (as returned by node editor)
	void draw(GraphState& state, uintptr_t selectedNodeRawId);
};

#endif
