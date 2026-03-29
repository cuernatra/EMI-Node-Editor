/** @file visualNode.h */
/** @brief Visual node data and rendering entry points. */

#ifndef VISUALNODE_H
#define VISUALNODE_H

#include "idGen.h"
#include "pin.h"
#include "link.h"
#include "../registry/fieldWidget.h"
#include <vector>
#include <string>

/** @brief Runtime instance of a graph node. */
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

#endif // VISUALNODE_H
