#ifndef APPCONSTANTS_H
#define APPCONSTANTS_H

#include <imgui.h>

struct Position
{
    float x;
    float y;
};

namespace appConstants
{
    const float windowWidth = 1280;
    constexpr int windowheight = 720;
}

namespace colors
{
    const ImVec4 green = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
    const ImVec4 gray  = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    const ImU32 blue = IM_COL32(120, 168, 179, 255);
}

namespace elementSizes
{
    const float topBarHeight = appConstants::windowheight / 12;
    const float dropBarHeight = appConstants::windowheight / 10;
    const float dropBarWidth = appConstants::windowheight / 10;
}

namespace elementLocations
{
    const Position dropBarLocation_A = {10.0f, 50.0f};
}

#endif