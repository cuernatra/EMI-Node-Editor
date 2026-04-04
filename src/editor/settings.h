#pragma once

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
    // Node header colors by category
    // -------------------------------------------------------------------------
    inline float nodeHeaderEventColorR = 0.220f;
    inline float nodeHeaderEventColorG = 0.340f;
    inline float nodeHeaderEventColorB = 0.760f;
    inline float nodeHeaderEventColorA = 1.000f;

    inline float nodeHeaderDataColorR = 0.180f;
    inline float nodeHeaderDataColorG = 0.520f;
    inline float nodeHeaderDataColorB = 0.420f;
    inline float nodeHeaderDataColorA = 1.000f;

    inline float nodeHeaderStructColorR = 0.470f;
    inline float nodeHeaderStructColorG = 0.340f;
    inline float nodeHeaderStructColorB = 0.700f;
    inline float nodeHeaderStructColorA = 1.000f;

    inline float nodeHeaderLogicColorR = 0.820f;
    inline float nodeHeaderLogicColorG = 0.520f;
    inline float nodeHeaderLogicColorB = 0.200f;
    inline float nodeHeaderLogicColorA = 1.000f;

    inline float nodeHeaderFlowColorR = 0.770f;
    inline float nodeHeaderFlowColorG = 0.300f;
    inline float nodeHeaderFlowColorB = 0.320f;
    inline float nodeHeaderFlowColorA = 1.000f;

    inline float nodeHeaderMoreColorR = 0.320f;
    inline float nodeHeaderMoreColorG = 0.320f;
    inline float nodeHeaderMoreColorB = 0.360f;
    inline float nodeHeaderMoreColorA = 1.000f;
 
    // Add new settings below using the same load/save pattern.
}