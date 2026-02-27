/**
 * @file fieldWidget.h
 * @brief Editable field widgets and runtime state management
 */

#ifndef FIELD_WIDGET_H
#define FIELD_WIDGET_H

#include "nodeDescriptor.h"
#include <string>

/**
 * @brief Editable field shown in node body (constant/parameter)
 * 
 * Values stored as strings; parsed on use for compilation.
 */
struct NodeField
{
    std::string name;       ///< Display label
    PinType     valueType;  ///< Data type (determines widget type)
    std::string value;      ///< Current value as string (parsed on use)
};

/**
 * @brief Render ImGui widget for field editing (modifies field in-place)
 * @return true if value changed this frame
 */
bool DrawField(NodeField& field);

/**
 * @brief Create runtime field from template descriptor (per-node instance from shared template)
 * 
 * Needed because descriptor is shared by all nodes of same type; each spawned node
 * must get its own field instance so edits don't affect other nodes of same type.
 */
inline NodeField MakeNodeField(const FieldDescriptor& desc)
{
    return { desc.name, desc.valueType, desc.defaultValue };
}

#endif // FIELD_WIDGET_H
