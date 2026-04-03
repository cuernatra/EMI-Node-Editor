#pragma once

#include "core/graph/nodeField.h"

#include <string>
#include <vector>

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

/// Render compact read-only preview for array fields in graph node rows.
void DrawArrayFieldPreviewReadOnly(const NodeField& field, int previewCount = 3);

/// Split a bracketed array string into top-level items.
std::vector<std::string> ParseArrayItems(const std::string& text);

/// Build a bracketed array string from already-trimmed items.
std::string BuildArrayString(const std::vector<std::string>& items);
