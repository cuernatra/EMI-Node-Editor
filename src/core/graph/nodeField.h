/** @file nodeField.h */
/** @brief Runtime field values stored on visual graph nodes. */

#ifndef NODE_FIELD_H
#define NODE_FIELD_H

#include "types.h"
#include <string>
#include <vector>

/**
 * @brief Editable field shown on a node instance.
 *
 * The descriptor defines the field shape, but each node stores its own live
 * value here so edits stay local to that instance.
 */
struct NodeField
{
    std::string name;       ///< Display label
    PinType     valueType;  ///< Data type (determines widget type)
    std::string value;      ///< Current value as string (parsed on use)
    std::vector<std::string> options; ///< Optional dropdown options from descriptor
};

/**
 * @brief Build a runtime field instance from a descriptor-like value.
 *
 * The input type must expose: name, valueType, defaultValue, options.
 */
template<typename FieldDescT>
inline NodeField MakeNodeField(const FieldDescT& desc)
{
    return { desc.name, desc.valueType, desc.defaultValue, desc.options };
}

#endif // NODE_FIELD_H