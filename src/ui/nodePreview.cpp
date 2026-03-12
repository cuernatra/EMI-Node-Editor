#include "nodePreview.h"
#include "../core/registry/nodeRegistry.h"
#include "../core/graph/pin.h"
#include "imgui.h"
#include "../app/constants.h"

void NodePreview::Draw(NodeType nodeType)
{
    const NodeRegistry& registry = NodeRegistry::Get();
    const NodeDescriptor* desc = registry.Find(nodeType);

    // Layout constants
    const float padding      = nodePreviewConstants::padding;
    const float headerHeight = nodePreviewConstants::headerHeight;
    const float fixedWidth   = nodePreviewConstants::fixedWidth;
    const float fixedHeight  = nodePreviewConstants::fixedHeight;
    const float pinRadius    = nodePreviewConstants::pinRadius;

    // Separate input and output pins
    std::vector<const PinDescriptor*> inputPins;
    std::vector<const PinDescriptor*> outputPins;
    for (const PinDescriptor& pd : desc->pins)
    {
        if (pd.isInput)
            inputPins.push_back(&pd);
        else
            outputPins.push_back(&pd);
    }

    // Dynamic pin spacing — more items means less space between them
    const float totalItems         = static_cast<float>(inputPins.size() + outputPins.size() + desc->fields.size());
    const float contentHeightAvail = fixedHeight - headerHeight - 2.0f * padding;
    const float pinSpacing         = totalItems > 1.0f ? contentHeightAvail / totalItems : contentHeightAvail;

    // Positioning
    const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImDrawList*  drawList  = ImGui::GetWindowDrawList();

    const ImU32 bgColor     = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 textColor   = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 pinColor    = ImGui::GetColorU32(ImGuiCol_TabActive);
    const ImU32 headerColor = ImGui::GetColorU32(ImGuiCol_HeaderHovered);

    const ImVec2 nodeMax(cursorPos.x + fixedWidth, cursorPos.y + fixedHeight);

    // Hover effect
    const bool  hovered         = ImGui::IsMouseHoveringRect(cursorPos, nodeMax);
    const ImU32 borderColor     = hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered) : textColor;
    const float expand          = hovered ? 1.0f : 0.0f;
    const float borderThickness = 1.0f + 2.0f * expand;

    // Node body
    drawList->AddRectFilled(cursorPos, nodeMax, bgColor, 2.0f);
    drawList->AddRect(
        ImVec2(cursorPos.x - expand, cursorPos.y - expand),
        ImVec2(nodeMax.x + expand, nodeMax.y + expand),
        borderColor, 2.0f, 0, borderThickness
    );

    // Header
    drawList->AddRectFilled(
        cursorPos,
        ImVec2(cursorPos.x + fixedWidth, cursorPos.y + headerHeight),
        headerColor,
        2.0f
    );
    drawList->AddText(ImVec2(cursorPos.x + padding, cursorPos.y + 3.0f), textColor, desc->label.c_str());

    // Content layout
    const float contentTop       = cursorPos.y + headerHeight + padding;
    const float totalItemsHeight = (totalItems - 1.0f) * pinSpacing;
    float       verticalOffset   = (contentHeightAvail - totalItemsHeight) / 2.0f;
    verticalOffset = std::max(verticalOffset, pinSpacing * 0.5f);

    // All text drawn with a single PushFont/PopFont pair
    ImGui::PushFont(nullptr);

    char   buf[128];
    size_t idx = 0;

    // Input pins
    for (const PinDescriptor* pin : inputPins)
    {
        const float  pinY   = contentTop + verticalOffset + static_cast<float>(idx) * pinSpacing;
        const ImVec2 pinPos(cursorPos.x + padding, pinY);
        drawList->AddCircleFilled(pinPos, pinRadius, pinColor);
        snprintf(buf, sizeof(buf), "-> %s", pin->name.c_str());
        drawList->AddText(ImVec2(pinPos.x + pinRadius + 4.0f, pinY - 6.0f), textColor, buf);
        ++idx;
    }

    // Fields
    for (const auto& field : desc->fields)
    {
        const float fieldY = contentTop + verticalOffset + static_cast<float>(idx) * pinSpacing;
        drawList->AddText(ImVec2(cursorPos.x + padding + 8.0f, fieldY - 6.0f), textColor, field.name.c_str());
        ++idx;
    }

    // Output pins
    for (const PinDescriptor* pin : outputPins)
    {
        const float  pinY   = contentTop + verticalOffset + static_cast<float>(idx) * pinSpacing;
        const ImVec2 pinPos(cursorPos.x + padding, pinY);
        drawList->AddCircleFilled(pinPos, pinRadius, pinColor);
        snprintf(buf, sizeof(buf), "%s ->", pin->name.c_str());
        drawList->AddText(ImVec2(pinPos.x + pinRadius + 4.0f, pinY - 6.0f), textColor, buf);
        ++idx;
    }

    ImGui::PopFont();

    ImGui::Dummy(ImVec2(fixedWidth, fixedHeight));
}