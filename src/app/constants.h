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
#include "../ui/theme.h"

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
 * @namespace elementSizes
 * @brief Dimensions for UI panels and components
 * 
 * Defines sizes relative to window dimensions for responsive layout.
 */
namespace elementSizes
{
    const float fileBarHeight = appConstants::windowheight / 20;   ///< Height of the file bar (36px)
    const float topBarHeight = appConstants::windowheight / 20;    ///< Height of top toolbar (36px)
}

/**
 * @namespace layoutConstants
 * @brief Layout tuning values for panel spacing, splitters, and limits.
 */
namespace layoutConstants
{
    const ImVec2 rootWindowPadding = ImVec2(0.0f, 0.0f);     ///< Root layout padding (panels stay flush)
    const ImVec2 rootItemSpacing = ImVec2(0.0f, 0.0f);       ///< Root item spacing (no gaps between panels)
    const ImVec2 panelPadding = ImVec2(10.0f, 10.0f);        ///< Internal content padding for each panel
    const ImVec2 panelItemSpacing = ImVec2(5.0f, 5.0f);      ///< Internal spacing between widgets in each panel

    const float horizontalSplitterWidth = 5.0f;              ///< Width of left/right splitter
    const float minLeftPanelWidth = 50.0f;                   ///< Minimum width for left panel
    const float minInspectorPanelWidth = 520.0f;             ///< Minimum width for inspector side

    const float consoleSplitterThickness = 5.0f;             ///< Thickness of console splitter
    const float minMainEditorHeight = 80.0f;                 ///< Minimum height for main editor area
    const float minConsoleHeight = 60.0f;                    ///< Minimum height for console area
    const float inspectorPaddingX = 8.0f;                    ///< Horizontal inset of overlay inspector
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