/**
 * @file theme.h
 * @brief Centralized UI theme color tokens.
 */

#ifndef UI_THEME_H
#define UI_THEME_H

#include <imgui.h>

/**
 * @namespace colors
 * @brief Color palette used throughout the UI.
 */
namespace colors
{
    const ImVec4 transparent = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);                                     ///< Fully transparent
    const ImVec4 background = ImVec4(18.0f / 255.0f, 18.0f / 255.0f, 18.0f / 255.0f, 1.0f);     ///< Background #121212
    const ImVec4 surface = ImVec4(24.0f / 255.0f, 24.0f / 255.0f, 24.0f / 255.0f, 1.0f);        ///< Surface #181818
    const ImVec4 elevated = ImVec4(40.0f / 255.0f, 40.0f / 255.0f, 40.0f / 255.0f, 1.0f);       ///< Elevated #282828
    const ImVec4 accent = ImVec4(78.0f / 255.0f, 161.0f / 255.0f, 255.0f / 255.0f, 1.0f);       ///< Accent blue #4EA1FFFF
    const ImVec4 textPrimary = ImVec4(212.0f / 255.0f, 212.0f / 255.0f, 212.0f / 255.0f, 1.0f); ///< Text Primary #D4D4D4 (VS Code-like)
    const ImVec4 textSecondary = ImVec4(140.0f / 255.0f, 140.0f / 255.0f, 140.0f / 255.0f, 1.0f); ///< Text Secondary #8C8C8C

    const ImVec4 green = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);        ///< Success/active state color
    const ImVec4 error = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);        ///< Error/failure state color
    const ImVec4 gray  = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);        ///< Disabled/inactive state color
    const ImVec4 lightGray = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);    ///< Secondary UI element color
    const ImVec4 topPanelButtonHover = ImVec4(32.0f / 255.0f, 32.0f / 255.0f, 32.0f / 255.0f, 1.0f); ///< Top panel button hover tone
    const ImU32 blue = IM_COL32(78, 161, 255, 255);             ///< Accent color for highlights
}

/**
 * @namespace uiFonts
 * @brief Shared ImGui font handles used by specific UI areas.
 */
namespace uiFonts
{
    inline ImFont* terminal = nullptr; ///< Monospace font for terminal/console logs
}

#endif
