#include "nodePreview.h"
#include "../core/registry/nodeRegistry.h"
#include "../core/graph/pin.h"
#include "imgui.h"

void NodePreview::Draw(NodeType nodeType)
{
    const NodeRegistry& registry = NodeRegistry::Get();
    const NodeDescriptor* desc = registry.Find(nodeType);

    // Layout constants
    constexpr float padding      = 8.0f;
    constexpr float pinRadius    = 3.5f;
    constexpr float headerHeight = 20.0f;
    constexpr float pinSpacing   = 18.0f;
    constexpr float pinGap       = 4.0f;

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

    // Compute layout dimensions — push font once for all CalcTextSize calls
    ImGui::PushFont(nullptr);

    char buf[128];
    float maxNameWidth = 0.0f;
    for (const PinDescriptor* pin : inputPins)
    {
        //maxNameWidth = std::max(maxNameWidth, ImGui::CalcTextSize(std::format("-> {}", pin->name).c_str()).x);
        snprintf(buf, sizeof(buf), "-> %s", pin->name.c_str());
        maxNameWidth = std::max(maxNameWidth, ImGui::CalcTextSize(buf).x);
    }
    for (const PinDescriptor* pin : outputPins)
    {
        //maxNameWidth = std::max(maxNameWidth, ImGui::CalcTextSize(std::format("{} ->", pin->name).c_str()).x);
        snprintf(buf, sizeof(buf), "%s ->", pin->name.c_str());
        maxNameWidth = std::max(maxNameWidth, ImGui::CalcTextSize(buf).x);
    }
    for (const auto& field : desc->fields)
        maxNameWidth = std::max(maxNameWidth, ImGui::CalcTextSize(field.name.c_str()).x);

    const float titleWidth = ImGui::CalcTextSize(desc->label.c_str()).x;

    ImGui::PopFont();

    float width = padding + pinRadius + pinGap + maxNameWidth + padding;
    width = std::max(width, titleWidth + 2.0f * padding);

    const float totalItems    = static_cast<float>(inputPins.size() + outputPins.size() + desc->fields.size());
    const float contentHeight = totalItems * pinSpacing;
    const float height        = headerHeight + 2.0f * padding + contentHeight;

    // Positioning
    const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImDrawList*  drawList  = ImGui::GetWindowDrawList();

    const ImU32 bgColor     = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 textColor   = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 pinColor    = ImGui::GetColorU32(ImGuiCol_TabActive);
    const ImU32 headerColor = ImGui::GetColorU32(ImGuiCol_HeaderHovered);

    const ImVec2 nodeMax(cursorPos.x + width, cursorPos.y + height);

    // hover effect when mouse is over the preview
    const bool hovered = ImGui::IsMouseHoveringRect(cursorPos, nodeMax);
    const ImU32 borderColor = hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered) : textColor;
    const float expand = hovered ? 1.0f : 0.0f; // edit this to change border hover effect
    const float borderThickness = 1.0f + 2 * expand;

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
        ImVec2(cursorPos.x + width, cursorPos.y + headerHeight),
        headerColor,
        2.0f
    );
    drawList->AddText(ImVec2(cursorPos.x + padding, cursorPos.y + 3.0f), textColor, desc->label.c_str());

    // Content layout
    const float contentTop             = cursorPos.y + headerHeight + padding;
    const float contentHeightAvailable = height - headerHeight - 2.0f * padding;
    const float totalItemsHeight = (totalItems - 1.0f) * pinSpacing;
    float       verticalOffset         = (contentHeightAvailable - totalItemsHeight) / 2.0f;
    verticalOffset = std::max(verticalOffset, pinSpacing * 0.5f);

    // All text drawn with a single PushFont/PopFont pair
    ImGui::PushFont(nullptr);

    size_t idx = 0;

    // Input pins
    for (const PinDescriptor* pin : inputPins)
    {
        const float  pinY   = contentTop + verticalOffset + static_cast<float>(idx) * pinSpacing;
        const ImVec2 pinPos(cursorPos.x + padding, pinY);
        drawList->AddCircleFilled(pinPos, pinRadius, pinColor);
        //drawList->AddText(ImVec2(pinPos.x + pinRadius + pinGap, pinY - 6.0f), textColor, std::format("-> {}", pin->name).c_str());
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
        //drawList->AddText(ImVec2(pinPos.x + pinRadius + pinGap, pinY - 6.0f), textColor, std::format("{} ->", pin->name).c_str());
        snprintf(buf, sizeof(buf), "%s ->", pin->name.c_str());
        drawList->AddText(ImVec2(pinPos.x + pinRadius + 4.0f, pinY - 6.0f), textColor, buf);
        ++idx;
    }

    ImGui::PopFont();

    ImGui::Dummy(ImVec2(width, height));
}