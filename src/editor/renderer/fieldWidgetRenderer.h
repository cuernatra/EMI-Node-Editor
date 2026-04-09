#pragma once

#include "core/graphModel/nodeField.h"

#include <string>
#include <vector>

enum class FieldWidgetLayout
{
	Compact,
	Inspector
};

/**
 * @brief Draw one editable field widget.
 * @return true if value changed this frame
 */
bool DrawField(NodeField& field);

/// Same as DrawField, with explicit layout.
bool DrawField(NodeField& field, FieldWidgetLayout layout);

/// Draw a read-only field value.
void DrawFieldReadOnly(const NodeField& field, FieldWidgetLayout layout);

/// Draw compact array preview in node rows.
void DrawArrayFieldPreviewReadOnly(const NodeField& field, int previewCount = 3);

/// Split "[...]" text into top-level items.
std::vector<std::string> ParseArrayItems(const std::string& text);

/// Build "[...]" text from item strings.
std::string BuildArrayString(const std::vector<std::string>& items);
