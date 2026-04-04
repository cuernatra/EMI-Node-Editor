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
struct NodeDescriptor;

/**
 * @brief Callback for compiling a visual node to AST
 * Eliminates need for large switch statements by storing compilation logic with the descriptor.
 */
using CompileCallback = std::function<Node*(GraphCompiler*, const VisualNode&)>;

/**
 * @brief Callback for deserializing a node from saved pin IDs.
 * Used by nodes with dynamic or non-standard pin layouts (Variable, Sequence, StructCreate).
 * Returns false on failure. nullptr = use standard pin-count-must-match path.
 */
using DeserializeCallback = std::function<bool(VisualNode&, const NodeDescriptor&, const std::vector<int>&)>;

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
    std::vector<std::string> options; ///< Optional fixed choice list for dropdown rendering
};

/**
 * @brief Optional palette representation override for a node type.
 * Allows one node type to appear multiple times in the palette with
 * different labels/payloads (e.g. Variable:Set, Variable:Get).
 */
struct PaletteVariant
{
    std::string displayLabel;  ///< Tile label shown in palette UI
    std::string payloadTitle;  ///< Drag payload title consumed by spawn logic
};

/**
 * @brief Layout hint used by the editor and renderer.
 *
 * The render style groups nodes by presentation shape so UI code can make
 * layout decisions without hardcoding NodeType checks.
 */
enum class NodeRenderStyle
{
    Default,
    Constant,
    Variable,
    Sequence,
    Delay,
    Loop,
    ForEach,
    Binary,
    Unary,
    Array,
    Draw,
    StructDefine,
    StructCreate,
    Function
};

/**
 * @brief Complete static definition of a node type
 * One immutable descriptor per NodeType; all instances share it and differ only in runtime state.
 * This is the structural contract for a node: pins, fields, palette entries, compile behavior,
 * deserialize behavior, and save token all live here.
 */
struct NodeDescriptor
{
    NodeType                     type;     ///< The node type this describes
    std::string                  label;    ///< Display title shown in node header
    std::vector<PinDescriptor>   pins;     ///< All pins in draw order (inputs then outputs)
    std::vector<FieldDescriptor> fields;   ///< Editable fields (empty for nodes without constants)
    CompileCallback              compile;      ///< Optional: Compiles this node to AST (nullptr = not compilable)
    DeserializeCallback          deserialize;  ///< Optional: Custom pin reconstruction from saved IDs (nullptr = standard exact-match path)
    std::string                  category = "More";  ///< Palette section name
    std::vector<PaletteVariant>  paletteVariants;     ///< Optional palette variants (empty = single default tile)
    std::string                  saveToken;  ///< Stable whitespace-free token used for serialization and spawn payloads
    std::vector<std::string>     deferredInputPins; ///< Input pins rendered inline with fields instead of in the top pin stack
    NodeRenderStyle              renderStyle = NodeRenderStyle::Default; ///< Presentation group used by the editor and renderer
};

#endif // NODE_DESCRIPTOR_H
