#ifndef APPCONSTANTS_H
#define APPCONSTANTS_H

#include <imgui.h>

namespace appConstants
{
    const float windowWidth = 1280;
    constexpr int windowheight = 720;
}

namespace colors
{
    const ImVec4 green = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
    const ImVec4 gray  = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    const ImU32 test = IM_COL32(120, 168, 179, 255);
}

struct Position
{
    float x;
    float y;
};

#endif