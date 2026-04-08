#pragma once

#include "visualNode.h"

/// Find an input pin by name.
///
/// @param n Node instance to search.
/// @param name Pin display/name string.
/// @return Pointer to pin within `n` or nullptr if not found.
inline const Pin* FindInputPin(const VisualNode& n, const char* name)
{
    for (const Pin& p : n.inPins)
        if (p.name == name)
            return &p;
    return nullptr;
}

/// Find an output pin by name.
///
/// @param n Node instance to search.
/// @param name Pin display/name string.
/// @return Pointer to pin within `n` or nullptr if not found.
inline const Pin* FindOutputPin(const VisualNode& n, const char* name)
{
    for (const Pin& p : n.outPins)
        if (p.name == name)
            return &p;
    return nullptr;
}

/// Find a node field by name.
///
/// @param n Node instance to search.
/// @param name Field name.
/// @return Pointer to the field value string within `n`, or nullptr.
inline const std::string* FindField(const VisualNode& n, const char* name)
{
    for (const NodeField& f : n.fields)
        if (f.name == name)
            return &f.value;
    return nullptr;
}

/// Resolve the fallback field used when an input pin is not connected.
///
/// This supports the "deferred input pin" pattern: when a pin is disconnected,
/// the editor may store a value in a field (often with the same name).
///
/// @param n Node instance.
/// @param pin The input pin whose fallback is being requested.
/// @param fieldName Optional override. If null/empty, uses `pin.name`.
/// @return Pointer to the field value string within `n`, or nullptr.
inline const std::string* FindInputFallbackField(const VisualNode& n, const Pin& pin, const char* fieldName = nullptr)
{
    if (fieldName && fieldName[0] != '\0')
        return FindField(n, fieldName);

    return FindField(n, pin.name.c_str());
}
