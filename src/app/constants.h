/**
 * @file constants.h
 * @brief Application-wide constants for UI layout, sizing, and colors
 * 
 * Central repository for all magic numbers used throughout the application.
 * Defines window dimensions, UI element sizes, color palettes, and positions.
 * 
 */

#ifndef APPCONSTANTS_H
#define APPCONSTANTS_H

#include <imgui.h>

/**
 * @brief Simple 2D position structure
 */
struct Position
{
    float x; ///< X coordinate
    float y; ///< Y coordinate
};

/**
 * @namespace appConstants
 * @brief Core application window dimensions
 */
namespace appConstants
{
    const float windowWidth = 1280;      ///< Default window width in pixels
    constexpr int windowheight = 720;    ///< Default window height in pixels
}

/**
 * @namespace colors
 * @brief Color palette used throughout the UI
 * 
 * Provides consistent colors for success states, disabled elements,
 * and UI accents.
 */
namespace colors
{
    const ImVec4 green = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);      ///< Success/active state color
    const ImVec4 gray  = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);      ///< Disabled/inactive state color
    const ImVec4 lightGray = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);  ///< Secondary UI element color
    const ImU32 blue = IM_COL32(120, 168, 179, 255);          ///< Accent color for highlights
}

/**
 * @namespace elementSizes
 * @brief Dimensions for UI panels and components
 * 
 * Defines sizes relative to window dimensions for responsive layout.
 */
namespace elementSizes
{
    const float topBarHeight = appConstants::windowheight / 20;    ///< Height of top toolbar (36px)
    const float dropBarHeight = appConstants::windowheight / 10;   ///< Height of drag-drop items (72px)
    const float dropBarWidth = appConstants::windowheight / 10;    ///< Width of drag-drop items (72px)
}

/**
 * @namespace elementLocations
 * @brief Fixed positions for UI elements
 * 
 * Defines absolute positions for UI components that don't use layout managers.
 */
namespace elementLocations
{
    const Position dropBarLocation_A = {10.0f, 50.0f};  ///< Position of first drop bar item
}

/**
 * @namespace nodePreviewConstants
 * @brief Constants specific to node preview rendering in the left panel.
 * 
 * Defines fixed sizes and layout parameters for the miniature node previews in the left panel.
 */
namespace nodePreviewConstants
{
    const float headerHeight = 20.0f;          ///< Height of the node preview header
    const float fixedWidth = 125.0f;           ///< Fixed width for all node previews
    const float fixedHeight = 110.0f;          ///< Fixed height for all node previews
    const float padding = 7.0f;                ///< Padding inside node previews
    const float pinRadius = 3.5f;              ///< Radius of the pin circles in node previews
}

namespace fontConstants
{
    const float fontSize = 15.0f;       ///< Default font size for ImGui elements
    const float oversampleH = 5.0f;     ///< Horizontal oversampling for font rendering
    const float oversampleV = 5.0f;     ///< Vertical oversampling for
}
#endif