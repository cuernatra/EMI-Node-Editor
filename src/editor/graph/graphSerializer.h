/** @file graphSerializer.h */
/** @brief Save/load graph state to a text file format. */

#pragma once

#include <string>

class GraphState;
struct VisualNode;

/** @brief Static helpers for graph serialization and deserialization. */
class GraphSerializer
{
public:
    /** @brief Write graph state to disk. */
    static void Save(const GraphState& state, const char* path);

    /** @brief Load graph state from disk. */
    static void Load(GraphState& state, const char* path);

    /** @brief Keep Constant node field/pin type data in sync. */
    static void ApplyConstantTypeFromFields(VisualNode& n, bool resetValueOnTypeChange = false);
};
