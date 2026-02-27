/**
 * @file visualNode.h
 * @brief Visual node representation for the node graph
 * 
 * Defines the runtime representation of a node in the visual editor.
 * Nodes contain pins (connection points), editable fields, and state
 * for positioning and lifecycle management.
 * 
 */

#ifndef VISUALNODE_H
#define VISUALNODE_H

#include "idGen.h"
#include "pin.h"
#include "../registry/fieldWidget.h"
#include <vector>
#include <string>

/**
 * @brief Runtime instance of a node in the visual graph
 * 
 * Represents a single node on the canvas with:
 * - Unique ID and type
 * - Collections of input and output pins
 * - Editable fields (for constant values, parameters)
 * - Position and lifecycle state
 * 
 * The node's structure (which pins/fields it has) is defined by its
 * NodeType's descriptor in the registry. This struct holds the live
 * runtime state.
 * 
 */
struct VisualNode
{
    ed::NodeId  id{};                         
    NodeType    nodeType  = NodeType::Unknown;  ///< Node type from registry

    std::vector<Pin>       inPins;    
    std::vector<Pin>       outPins;   ///< Output pins (branch/loop may have multiple)
    std::vector<NodeField> fields;    ///< Editable constant/parameter values

    std::string title;                ///< Display name shown in node header

    ImVec2 initialPos{0, 0}; 
    bool   positioned = false; 
    bool   alive      = true;         ///< false marks node for deletion
};

/**
 * @brief Render a single pin on a node
 * @param pin The pin to render
 * 
 * Draws an individual connection point using imgui-node-editor.
 * Must be called within an ed::BeginNode() / ed::EndNode() block.
 */
static void DrawPin(const Pin& pin);

/**
 * @brief Render a visual node on the canvas
 * @param n The node to render
 * 
 * Draws the node using imgui-node-editor primitives:
 * - Node body with title
 * - Input pins on the left
 * - Output pins on the right
 * - Editable fields in the center
 * 
 * Must be called within an ed::Begin() / ed::End() block.
 */
bool DrawVisualNode(VisualNode& n);

#endif // VISUALNODE_H
