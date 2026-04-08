/** @file nodeDescriptor.h */
/** @brief Describes each node type: its pins, fields, and optional build hooks. */

#ifndef NODE_DESCRIPTOR_H
#define NODE_DESCRIPTOR_H

#include "../graph/types.h"
#include <string>
#include <vector>
#include <functional>

// Forward declarations used by the optional hooks below.
class GraphCompiler;
class Node;
struct VisualNode;
struct NodeDescriptor;

/** @brief Build this node into the script representation. */
using CompileCallback = std::function<Node*(GraphCompiler*, const VisualNode&)>;

/**
 * @brief Build a node that controls execution (if/loop/sequence/etc.).
 *
 * Appends statements into the provided block and continues along the node's
 * flow outputs.
 */
using CompileFlowCallback = std::function<void(GraphCompiler*, const VisualNode&, Node* targetScope)>;

/** @brief Build the value for one specific output pin (return nullptr to fall back). */
using CompileOutputCallback = std::function<Node*(GraphCompiler*, const VisualNode&, int outPinIdx)>;

/**
 * @brief Add declarations that must exist before the main run path.
 *
 * Example: variable declarations or temporary variables.
 */
using CompilePreludeCallback = std::function<void(GraphCompiler*, const VisualNode&, Node* rootScope, Node* graphBodyScope)>;

/**
 * @brief Add a top-level definition (for example, a function definition).
 *
 * Return nullptr to add nothing.
 */
using CompileTopLevelCallback = std::function<Node*(GraphCompiler*, const VisualNode&)>;

/**
 * @brief Custom load for nodes with unusual pins (or legacy formats).
 *
 * Return false on failure. If unset, default loading is used.
 */
using DeserializeCallback = std::function<bool(VisualNode&, const NodeDescriptor&, const std::vector<int>&)>;

/** @brief One pin in a node template. */
struct PinDescriptor
{
    std::string name;          ///< Display name.
    PinType     type;          ///< Value type.
    bool        isInput;       ///< true = input pin, false = output pin.
    bool        isMultiInput = false;  ///< If true, allow multiple incoming links.
};

/** @brief One editable field shown in the inspector. */
struct FieldDescriptor
{
    std::string name;          ///< Display name.
    PinType     valueType;     ///< Value type.
    std::string defaultValue;  ///< Default value (stored as text).
    std::vector<std::string> options; ///< Optional dropdown options.
};

/**
 * @brief Extra "button" in the node palette for the same NodeType.
 *
 * Useful when one node type has multiple flavors at spawn time.
 * Example: show "Set Variable" and "Get Variable" as separate palette items,
 * but both create NodeType::Variable with different initial field values.
 */
struct PaletteVariant
{
    std::string displayLabel;  ///< Text shown in the palette.
    std::string payloadTitle;  ///< Drag/drop payload used when spawning.
};

/** @brief Rendering/grouping hint used by the editor. */
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

/** @brief Full template for one node type. */
struct NodeDescriptor
{
    NodeType                     type;     ///< Which node this describes.
    std::string                  label;    ///< Display name shown in the UI.
    std::vector<PinDescriptor>   pins;     ///< Pins (in UI order).
    std::vector<FieldDescriptor> fields;   ///< Editable fields shown in the inspector.

    // ── Compile callbacks ─────────────────────────────────────────────────────
    // Exactly one of `compile` or `compileFlow` should be set per node type.
    // The others are optional add-ons. Quick guide:
    //
    //   compile         — node produces a value or a single statement
    //                     (operators, set-variable, print, draw, call-function, …)
    //
    //   compileFlow     — node controls which code runs next
    //                     (Branch, Loop, While, Sequence, ForEach)
    //                     The callback must call AppendFlowChainFromOutput() on
    //                     every downstream flow output it wants to continue.
    //
    //   compileOutput   — node has multiple output pins that carry different values
    //                     (return non-null to override; null falls back to compile)
    //                     Example: Loop → Index pin vs LastIndex pin
    //
    //   compilePrelude  — node needs something declared before the main body runs
    //                     (Variable → var-declare, Loop → temp vars, CallFunction → result temp)
    //
    //   compileTopLevel — node emits a definition at global scope
    //                     (user-defined Function → FunctionDef AST node)
    // ─────────────────────────────────────────────────────────────────────────
    CompileCallback              compile;        ///< Build this node as a value or statement.
    CompileFlowCallback          compileFlow;    ///< Build flow behavior (node controls execution).
    CompileOutputCallback        compileOutput;  ///< Build one specific output pin value.
    CompilePreludeCallback       compilePrelude; ///< Emit declarations before the main run path.
    CompileTopLevelCallback      compileTopLevel; ///< Emit a top-level definition (e.g. a function).
    DeserializeCallback          deserialize;    ///< Custom load when pins/fields are unusual.

    /**
     * @brief Skip the flow-reachability guard when reading this node's output pins.
     *
     * Normally, if a data wire comes from a node that also has flow pins, the
     * compiler checks that the source node has been reached via execution wires
     * before trusting its output. If it hasn't run yet, a safe default is returned
     * instead.
     *
     * Set this to true when that guard is overly conservative. The canonical
     * example is **CallFunction**: its Result pin carries a value that was stored
     * in a pre-declared temp variable during the Prelude stage (Stage 3), so the
     * value is always valid regardless of where the node sits in the flow graph.
     * The flow-reachability check would wrongly return a default here, so it must
     * be bypassed.
     */
    bool                         bypassFlowReachableCheck = false;

    std::string                  category = "More";  ///< Palette group name (also used for header colors).
    std::vector<PaletteVariant>  paletteVariants;     ///< Extra palette entries for this NodeType.
    std::string                  saveToken;  ///< Stable identifier used when saving/loading.
    std::vector<std::string>     deferredInputPins; ///< Input pins shown inline with fields (UI-only).
    NodeRenderStyle              renderStyle = NodeRenderStyle::Default; ///< UI rendering group.
};

#endif // NODE_DESCRIPTOR_H
