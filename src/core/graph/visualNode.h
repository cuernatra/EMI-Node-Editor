/** @file visualNode.h */
/** @brief Runtime node data used by the graph editor and compiler. */

#ifndef VISUALNODE_H
#define VISUALNODE_H

#include "idGen.h"
#include "pin.h"
#include "nodeField.h"
#include <vector>
#include <string>

/** @brief One live node instance in the graph. */
struct VisualNode
{
    ed::NodeId  id{};                         
    NodeType    nodeType  = NodeType::Unknown;  ///< Node type.

    std::vector<Pin>       inPins;    
    std::vector<Pin>       outPins;   ///< Output pins.
    std::vector<NodeField> fields;    ///< Editable field values.

    std::string title;                ///< Node title shown in UI.

    ImVec2 initialPos{0, 0}; 
    bool   positioned = false; 
    bool   alive      = true;         ///< False means logically deleted.
};

// Drawing code lives in editor/renderer.

#endif // VISUALNODE_H
