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
 
    // -------------------------------------------------------------------------
    // Add new settings below, following the same pattern
    // -------------------------------------------------------------------------
}