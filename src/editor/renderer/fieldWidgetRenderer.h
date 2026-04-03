#pragma once

#include "core/registry/fieldWidget.h"

enum class FieldWidgetLayout
{
	Compact,
	Inspector
};

/**
 * @brief Render ImGui widget for field editing (modifies field in-place)
 * @return true if value changed this frame
 */
bool DrawField(NodeField& field);

/// Same as DrawField(field), but with explicit layout choice.
bool DrawField(NodeField& field, FieldWidgetLayout layout);

/// Render a read-only view of a field (no edits).
void DrawFieldReadOnly(const NodeField& field, FieldWidgetLayout layout);
