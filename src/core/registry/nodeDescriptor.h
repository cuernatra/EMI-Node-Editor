/** @file nodeDescriptor.h */
/** @brief Static node templates used to build runtime node instances. */

#ifndef NODE_DESCRIPTOR_H
#define NODE_DESCRIPTOR_H

#include "../graph/types.h"
#include <string>
#include <vector>
#include <functional>

// Forward declarations used by callbacks.
class GraphCompiler;
class Node;
struct VisualNode;
struct NodeDescriptor;

/**
 * @brief Callback that compiles one node into AST.
 *
 * Storing compile logic on the descriptor avoids large switch statements.
 */
using CompileCallback = std::function<Node*(GraphCompiler*, const VisualNode&)>;

/**
 * @brief Callback for loading nodes that use non-standard pin layouts.
 *
 * Return false on failure. nullptr means use default exact-pin-count loading.
 */
using DeserializeCallback = std::function<bool(VisualNode&, const NodeDescriptor&, const std::vector<int>&)>;

/**
 * @brief Static definition of one pin in a node template.
 */
struct PinDescriptor
{
    std::string name;          ///< Pin label.
    PinType     type;          ///< Pin data type.
    bool        isInput;       ///< true=input, false=output.
    bool        isMultiInput = false;  ///< Allow multiple incoming links.
};

/**
 * @brief Static definition of one editable field in a node template.
 */
struct FieldDescriptor
{
    std::string name;          ///< Field label.
    PinType     valueType;     ///< Field data type.
    std::string defaultValue;  ///< Default text value.
    std::vector<std::string> options; ///< Optional dropdown options.
};

/**
 * @brief Optional extra palette tile for a node type.
 *
 * Lets one type appear multiple times with different labels/payloads.
 */
struct PaletteVariant
{
    std::string displayLabel;  ///< Tile label.
    std::string payloadTitle;  ///< Drag payload title.
};

/**
 * @brief Render grouping hint used by editor UI.
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
    StructCreate
};

/**
 * @brief Full template for one node type.
 */
struct NodeDescriptor
{
    NodeType                     type;     ///< Node type key.
    std::string                  label;    ///< Node header label.
    std::vector<PinDescriptor>   pins;     ///< Pins in draw order.
    std::vector<FieldDescriptor> fields;   ///< Editable fields.
    CompileCallback              compile;      ///< Optional compile callback.
    DeserializeCallback          deserialize;  ///< Optional custom load callback.
    std::string                  category = "More";  ///< Palette category.
    std::vector<PaletteVariant>  paletteVariants;     ///< Optional extra palette tiles.
    std::string                  saveToken;  ///< Stable save token.
    std::vector<std::string>     deferredInputPins; ///< Inputs rendered inline with fields.
    NodeRenderStyle              renderStyle = NodeRenderStyle::Default; ///< UI render group.
};

#endif // NODE_DESCRIPTOR_H
