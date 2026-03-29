#pragma once

/**
 * @file settings.h
 * @brief Persistent application settings
 *
 * Loads and saves settings to settings.json in the working directory.
 * Call Settings::Load() on startup and Settings::Save() on shutdown.
 *
 * Adding a new setting:
 *   1. Add an inline variable with a sensible default value
 *   2. Add a line in Load() using s.value("key", defaultValue)
 *   3. Add a line in Save() using s["key"] = member
 */
namespace Settings
{
    void Load();
    void Save();
 
    // -------------------------------------------------------------------------
    // Layout
    // -------------------------------------------------------------------------
    inline float leftPanelWidth = -1.0f;  ///< Width of the left panel in pixels

    inline float consolePanelHeight = -1.0f; ///< Height of the console panel in pixels

    inline bool consolePanelIsMinimized = false; ///< Whether the console panel is currently minimized

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
    // Add new settings below, following the same pattern
    // -------------------------------------------------------------------------
}