/** @file nodeField.h */
/** @brief Runtime field values stored on visual graph nodes. */

#ifndef NODE_FIELD_H
#define NODE_FIELD_H

#include "types.h"
#include <string>
#include <vector>

/**
 * @brief Editable value stored on a node instance.
 *
 * Each node keeps its own value, so changing one node does not change others.
 */
struct NodeField
{
    std::string name;       ///< Display label
    PinType     valueType;  ///< Data type (determines widget type)
    std::string value;      ///< Current value as string (parsed on use)
    std::vector<std::string> options; ///< Optional dropdown options from descriptor
};

/**
 * @brief Build a node field from descriptor data.
 *
 * The input type must expose: name, valueType, defaultValue, options.
 */
template<typename FieldDescT>
inline NodeField MakeNodeField(const FieldDescT& desc)
{
    return { desc.name, desc.valueType, desc.defaultValue, desc.options };
}

#endif // NODE_FIELD_H