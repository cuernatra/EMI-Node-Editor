#include "pinRenderer.h"

#include <algorithm>
#include <array>

namespace
{
    PinStylePalette g_pinStylePalette{};
    PinIconPalette  g_pinIconPalette{};
    EditorPinStyle  g_editorPinStyle{};
}

const PinStylePalette& GetPinStylePalette()
{
    return g_pinStylePalette;
}

void SetPinStylePalette(const PinStylePalette& palette)
{
    g_pinStylePalette = palette;
}

ImVec4 GetPinTypeColor(PinType type)
{
    const PinStylePalette& palette = GetPinStylePalette();

    switch (type)
    {
        case PinType::Number:   return palette.number;
        case PinType::Boolean:  return palette.boolean;
        case PinType::String:   return palette.string;
        case PinType::Array:    return palette.array;
        case PinType::Struct:   return palette.struct_;
        case PinType::Function: return palette.function;
        case PinType::Flow:     return palette.flow;
        case PinType::Any:
        default:                return palette.any;
    }
}

const PinIconPalette& GetPinIconPalette()
{
    return g_pinIconPalette;
}

void SetPinIconPalette(const PinIconPalette& palette)
{
    g_pinIconPalette = palette;
}

PinIconType GetPinTypeIcon(PinType type)
{
    const PinIconPalette& palette = GetPinIconPalette();

    switch (type)
    {
        case PinType::Number:   return palette.number;
        case PinType::Boolean:  return palette.boolean;
        case PinType::String:   return palette.string;
        case PinType::Array:    return palette.array;
        case PinType::Struct:   return palette.struct_;
        case PinType::Function: return palette.function;
        case PinType::Flow:     return palette.flow;
        case PinType::Any:
        default:                return palette.any;
    }
}

void DrawPinIcon(ImDrawList* drawList,
                 const ImVec2& a,
                 const ImVec2& b,
                 PinIconType type,
                 bool filled,
                 ImU32 color,
                 ImU32 innerColor)
{
    if (drawList == nullptr)
        return;

    const float minX = (a.x < b.x) ? a.x : b.x;
    const float minY = (a.y < b.y) ? a.y : b.y;
    const float maxX = (a.x > b.x) ? a.x : b.x;
    const float maxY = (a.y > b.y) ? a.y : b.y;
    const float w = maxX - minX;
    const float h = maxY - minY;
    const float s = std::min(w, h);
    const ImVec2 c((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
    const float outlineScale = std::max(0.5f, w / 24.0f);
    const int extraSegments = static_cast<int>(2.0f * outlineScale);

    switch (type)
    {
        case PinIconType::Circle:
        {
            const float r = 0.25f * s;
            if (!filled && (innerColor & 0xFF000000))
                drawList->AddCircleFilled(c, r, innerColor, 12 + extraSegments);

            if (filled)
                drawList->AddCircleFilled(c, r, color, 12 + extraSegments);
            else
                drawList->AddCircle(c, r, color, 12 + extraSegments, 2.0f * outlineScale);
            break;
        }
        case PinIconType::Flow:
        {
            const float offsetX = 1.0f * outlineScale;
            const float offsetY = 0.0f * outlineScale;
            const float margin = 2.0f * outlineScale;
            const float rounding = 0.1f * outlineScale;
            const float tipRound = 0.7f;

            const float canvasMinX = minX + margin + offsetX;
            const float canvasMinY = minY + margin + offsetY;
            const float canvasMaxX = maxX - margin + offsetX;
            const float canvasMaxY = maxY - margin + offsetY;
            const float canvasW = canvasMaxX - canvasMinX;
            const float canvasH = canvasMaxY - canvasMinY;

            const float left = canvasMinX + canvasW * 0.5f * 0.3f;
            const float right = canvasMinX + canvasW - canvasW * 0.5f * 0.3f;
            const float top = canvasMinY + canvasH * 0.5f * 0.2f;
            const float bottom = canvasMinY + canvasH - canvasH * 0.5f * 0.2f;
            const float centerY = (top + bottom) * 0.5f;

            const ImVec2 tipTop(canvasMinX + canvasW * 0.5f, top);
            const ImVec2 tipRight(right, centerY);
            const ImVec2 tipBottom(canvasMinX + canvasW * 0.5f, bottom);
            const ImVec2 leftTop(left, top);
            const ImVec2 leftBottom(left, bottom);
            const ImVec2 leftTopRounded(left, top + rounding);
            const ImVec2 leftTopOut(left + rounding, top);
            const ImVec2 leftBottomRounded(left + rounding, bottom);
            const ImVec2 leftBottomOut(left, bottom - rounding);
            const ImVec2 tipTopRound(
                tipTop.x + (tipRight.x - tipTop.x) * tipRound,
                tipTop.y + (tipRight.y - tipTop.y) * tipRound);
            const ImVec2 tipBottomRound(
                tipBottom.x + (tipRight.x - tipBottom.x) * tipRound,
                tipBottom.y + (tipRight.y - tipBottom.y) * tipRound);

            const auto buildFlowPath = [&]() {
                drawList->PathLineTo(leftTopRounded);
                drawList->PathBezierCubicCurveTo(
                    leftTop,
                    leftTop,
                    leftTopOut,
                    0);
                drawList->PathLineTo(tipTop);
                drawList->PathLineTo(tipTopRound);
                drawList->PathBezierCubicCurveTo(
                    tipRight,
                    tipRight,
                    tipBottomRound,
                    0);
                drawList->PathLineTo(tipBottom);
                drawList->PathLineTo(leftBottomRounded);
                drawList->PathBezierCubicCurveTo(
                    leftBottom,
                    leftBottom,
                    leftBottomOut,
                    0);
            };

            if (!filled)
            {
                if (innerColor & 0xFF000000)
                {
                    buildFlowPath();
                    drawList->PathFillConvex(innerColor);
                }

                buildFlowPath();
                drawList->PathStroke(color, true, 2.0f * outlineScale);
            }
            else
            {
                buildFlowPath();
                drawList->PathFillConvex(color);
            }
            break;
        }
        case PinIconType::Diamond:
        {
            const float r = 0.36f * s;
            const ImVec2 top(c.x, c.y - r);
            const ImVec2 right(c.x + r, c.y);
            const ImVec2 bottom(c.x, c.y + r);
            const ImVec2 left(c.x - r, c.y);

            if (!filled && (innerColor & 0xFF000000))
            {
                drawList->AddConvexPolyFilled(
                    std::array<ImVec2, 4>{ top, right, bottom, left }.data(),
                    4,
                    innerColor
                );
            }

            if (filled)
            {
                drawList->AddConvexPolyFilled(
                    std::array<ImVec2, 4>{ top, right, bottom, left }.data(),
                    4,
                    color
                );
            }
            else
            {
                drawList->AddPolyline(
                    std::array<ImVec2, 5>{ top, right, bottom, left, top }.data(),
                    5,
                    color,
                    false,
                    2.0f * outlineScale
                );
            }
            break;
        }
        default:
            break;
    }
}

const EditorPinStyle& GetEditorPinStyle()
{
    return g_editorPinStyle;
}

void SetEditorPinStyle(const EditorPinStyle& style)
{
    g_editorPinStyle = style;
}

void ApplyEditorPinStyle(ed::Style& style)
{
    const EditorPinStyle& pinStyle = GetEditorPinStyle();
    style.PinRounding = pinStyle.pinRounding;
    style.PinBorderWidth = pinStyle.pinBorderWidth;
    style.PinRadius = pinStyle.pinRadius;
    style.PinArrowSize = pinStyle.pinArrowSize;
    style.PinArrowWidth = pinStyle.pinArrowWidth;
}
