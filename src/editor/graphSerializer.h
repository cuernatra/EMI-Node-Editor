/**
 * @file graphSerializer.h
 * @brief Graph persistence - save/load to disk
 * 
 * Handles serialization and deserialization of graph state to/from
 * plain text files. Uses a simple custom format for storing nodes,
 * links, and their properties.
 * 
 */

#pragma once

#include <string>

class GraphState;

/**
 * @brief Graph file I/O operations
 * 
 * Provides static methods for saving and loading complete graph state
 * to/from disk. The file format is a simple text-based representation
 * that stores:
 * - Node IDs, types, positions, and field values
 * - Pin IDs and their associations
 * - Link IDs and connections
 * - Next available ID for the generator
 * 
 */
class GraphSerializer
{
public:
    /**
     * @brief Save graph to file
     * @param state The graph state to save
     * @param path File path for output
     * 
     * Serializes all nodes, links, and ID state to a text file.
     * Creates or overwrites the file at the given path.
     */
    static void Save(const GraphState& state, const char* path);

    /**
     * @brief Load graph from file
     * @param state The graph state to populate (will be cleared first)
     * @param path File path to load from
     * 
     * Deserializes a graph from disk, clearing any existing state.
     * If the file doesn't exist or is malformed, state is left empty.
     */
    static void Load(GraphState& state, const char* path);
};
