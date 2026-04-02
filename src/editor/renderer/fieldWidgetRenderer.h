#pragma once

#include "core/registry/fieldWidget.h"

/**
 * @brief Render ImGui widget for field editing (modifies field in-place)
 * @return true if value changed this frame
 */
bool DrawField(NodeField& field);
