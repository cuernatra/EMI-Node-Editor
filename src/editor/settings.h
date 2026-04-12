#pragma once

#include <array>
#include <string>
#include <unordered_map>

/**
 * @file settings.h
 * @brief Persistent editor settings.
 *
 * Loads/saves values from settings.json in the working directory.
 * Call Settings::Load() on startup and Settings::Save() on shutdown.
 *
 * Adding a new setting:
 *   1. Add an inline variable with a sensible default value
 *   2. Add a line in Load() using s.value("key", defaultValue)
 *   3. Add a line in Save() using s["key"] = member
 *
 * Node header colors are stored dynamically by category string.
 * Any new category registered in the NodeRegistry automatically gets a color.
 * Use GetNodeHeaderColor(category) to look up or generate a default color.
 */
namespace Settings
{
    void Load();
    void Save();

    // Returns the RGBA color for a node header category.
    // Generates and stores a default if the category is not yet known.
    std::array<float, 4> GetNodeHeaderColor(const std::string& category);

    // -------------------------------------------------------------------------
    // Layout
    // -------------------------------------------------------------------------
    inline float leftPanelWidth = -1.0f;  ///< Width of the left panel in pixels

    inline float consolePanelHeight = -1.0f; ///< Height of the console panel in pixels

    inline bool consolePanelIsMinimized = true; ///< Whether the console panel is currently minimized

    // -------------------------------------------------------------------------
    // Graph canvas colors
    // -------------------------------------------------------------------------
    inline float gridBgColorR = 0.235f;
    inline float gridBgColorG = 0.235f;
    inline float gridBgColorB = 0.275f;
    inline float gridBgColorA = 0.784f;

    inline float gridLineColorR = 0.470f;
    inline float gridLineColorG = 0.470f;
    inline float gridLineColorB = 0.470f;
    inline float gridLineColorA = 0.157f;

    // -------------------------------------------------------------------------
    // Node header colors by category (dynamic — populated from the registry)
    // -------------------------------------------------------------------------
    // Keyed by the descriptor category string (e.g. "Events", "Data", "Math").
    // Do not read this map directly; use GetNodeHeaderColor() instead.
    inline std::unordered_map<std::string, std::array<float, 4>> nodeHeaderColors;

    // Add new settings below using the same load/save pattern.
}