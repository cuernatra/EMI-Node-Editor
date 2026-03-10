#include "nodePreview.h"
#include "../core/registry/nodeRegistry.h"
#include "../core/graph/pin.h"
#include "imgui.h"
#include <format>

void NodePreview::Draw(NodeType nodeType, float maxWidth, float maxHeight)
{
    const NodeRegistry& registry = NodeRegistry::Get();
    const NodeDescriptor* desc = registry.Find(nodeType);

    // Layout constants
    float padding = 8.0f;
    float pinRadius = 3.5f;
    float headerHeight = 20.0f;
    float minPinSpacing = 18.0f;

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

    // Compute width by measuring longest pin name or title
    ImGui::PushFont(nullptr);
    float maxNameWidth = 0.0f;
    for (auto* pin : inputPins)
        maxNameWidth = std::max(maxNameWidth, ImGui::CalcTextSize(("-> " + std::string(pin->name)).c_str()).x);
    for (auto* pin : outputPins)
        maxNameWidth = std::max(maxNameWidth, ImGui::CalcTextSize((std::string(pin->name) + " ->").c_str()).x);
    ImVec2 titleSize = ImGui::CalcTextSize(desc->label.c_str());
    ImGui::PopFont();

    float pinGap = 4.0f;
    float width = padding + pinRadius + pinGap + maxNameWidth + padding;
    width = std::max(width, titleSize.x + 2 * padding);

    // calculate height stacking all pins
    float totalPins = inputPins.size() + outputPins.size();
    float contentHeight = totalPins > 0 ? totalPins * minPinSpacing + minPinSpacing : minPinSpacing;
    float height = headerHeight + 2 * padding + contentHeight;

    // Get current cursor position for drawing
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_FrameBg);
    ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
    ImU32 pinColor = ImGui::GetColorU32(ImGuiCol_TabActive);
    ImU32 headerColor = ImGui::GetColorU32(ImGuiCol_HeaderHovered);

    // Draw node body background
    drawList->AddRectFilled(
        cursorPos,
        ImVec2(cursorPos.x + width, cursorPos.y + height),
        bgColor,
        2.0f  // Rounding
    );

    // Draw node border
    drawList->AddRect(
        cursorPos,
        ImVec2(cursorPos.x + width, cursorPos.y + height),
        textColor,
        2.0f,  // Rounding
        0,
        1.0f  // Thickness
    );

    // Draw header background
    drawList->AddRectFilled(
        ImVec2(cursorPos.x, cursorPos.y),
        ImVec2(cursorPos.x + width, cursorPos.y + headerHeight),
        headerColor,
        2.0f
    );

    // Draw title text (centered vertically in header)
    drawList->AddText(
        ImVec2(cursorPos.x + padding, cursorPos.y + 3.0f),
        textColor,
        desc->label.c_str()
    );

    float contentTop = cursorPos.y + headerHeight + padding;
    float contentHeightAvailable = height - headerHeight - 2 * padding;
    
    // Calculate pin spacing: ensure minimum space between pins and center them
    float pinSpacing = minPinSpacing;
    float totalPinsHeight = totalPins > 0 ? (totalPins - 1) * pinSpacing : 0;
    float verticalOffset = (contentHeightAvailable - totalPinsHeight) / 2.0f;
    verticalOffset = std::max(verticalOffset, pinSpacing * 0.5f);  // At least some offset from header
    // Draw input pins first
    for (size_t i = 0; i < inputPins.size(); ++i)
    {
        float pinY = contentTop + verticalOffset + i * pinSpacing;
        ImVec2 pinPos(cursorPos.x + padding, pinY);
        drawList->AddCircleFilled(pinPos, pinRadius, pinColor);
        ImGui::PushFont(nullptr);
        drawList->AddText(
            ImVec2(pinPos.x + pinRadius + 4.0f, pinY - 6.0f),
            textColor,
            std::format("-> {}", inputPins[i]->name).c_str()
        );
        ImGui::PopFont();
    }

    // Draw all pins in sequence: inputs then outputs (single column)
    size_t idx = inputPins.size(); // after inputs continue
    for (size_t i = 0; i < outputPins.size(); ++i, ++idx)
    {
        float pinY = contentTop + verticalOffset + idx * pinSpacing;
        ImVec2 pinPos(cursorPos.x + padding, pinY);
        drawList->AddCircleFilled(pinPos, pinRadius, pinColor);
        ImGui::PushFont(nullptr);
        drawList->AddText(
            ImVec2(pinPos.x + pinRadius + 4.0f, pinY - 6.0f),
            textColor,
            std::format("{} ->", outputPins[i]->name).c_str()
        );
        ImGui::PopFont();
    }

    // Draw field indicators (small labels if fields exist)
    // Omitted for now - field indicator was causing visual clutter
    // Could be added at bottom if needed

    // Dummy item for ImGui layout
    ImGui::Dummy(ImVec2(width, height));
}
