/**
 * @file nodeDescriptor.h
 * @brief Static node type definitions and descriptors
 * 
 * Defines the structure of node types via descriptors that specify:
 * - Pin configuration (names, types, directions)
 * - Editable fields (names, types, defaults)
 * - Display information (labels)
 * 
 * These descriptors are templates used by the factory to create node instances.
 * 
 */

#ifndef NODE_DESCRIPTOR_H
#define NODE_DESCRIPTOR_H

#include "../graph/types.h"
#include <string>
#include <vector>
#include <functional>

// Forward declarations for compile callback
class GraphCompiler;
class Node;
struct VisualNode;

// TODO: tarviiks tätä

/**
 * @brief Callback for compiling a visual node to AST
 * Eliminates need for large switch statements by storing compilation logic with the descriptor.
 */
using CompileCallback = std::function<Node*(GraphCompiler*, const VisualNode&)>;

/**
 * @brief Static definition of a pin on a node type
 * Used by factory to create Pin objects with correct name, type, and direction.
 */
struct PinDescriptor
{
    std::string name;          ///< Display label for the pin
    PinType     type;          ///< Data type the pin handles
    bool        isInput;       ///< true = input, false = output
    bool        isMultiInput = false;  ///< Allow multiple incoming connections
};

/**
 * @brief Static definition of an editable field on a node type
 * valueType determines the ImGui widget: Number→InputFloat, Boolean→Checkbox, String→InputText.
 */
struct FieldDescriptor
{
    std::string name;          ///< Display label for the field
    PinType     valueType;     ///< Data type (drives widget selection)
    std::string defaultValue;  ///< Initial value (stored as string, parsed on use)
};

/**
 * @brief Complete static definition of a node type
 * One immutable descriptor per NodeType; all instances share it and differ only in runtime state.
 */
struct NodeDescriptor
{
    NodeType                     type;     ///< The node type this describes
    std::string                  label;    ///< Display title shown in node header
    std::vector<PinDescriptor>   pins;     ///< All pins in draw order (inputs then outputs)
    std::vector<FieldDescriptor> fields;   ///< Editable fields (empty for nodes without constants)
    CompileCallback              compile;  ///< Optional: Compiles this node to AST (nullptr = not compilable)
    bool                         supportsVariablePins = false;  ///< Nodes that accept variable pin counts (e.g., Sequence, Variable, StructCreate)
};

#endif // NODE_DESCRIPTOR_H
