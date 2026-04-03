/** @file visualNode.h */
/** @brief Visual node data and rendering entry points. */

#ifndef VISUALNODE_H
#define VISUALNODE_H

#include "idGen.h"
#include "pin.h"
#include "nodeField.h"
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

// Rendering helpers live on the editor side (see editor/renderer/nodeRenderer.h).

#endif // VISUALNODE_H
