#include "nodePreview.h"
#include "core/registry/nodeRegistry.h"
#include "core/graph/pin.h"
#include "editor/nodeColorCategories.h"
#include "editor/settings.h"
#include "imgui.h"
#include "app/constants.h"
#include <string>

namespace
{
ImVec4 GetNodeCategoryHeaderColor(NodeType nodeType)
{
    switch (GetNodeColorCategory(nodeType))
    {
        case NodeColorCategory::Event:
            return ImVec4(Settings::nodeHeaderEventColorR, Settings::nodeHeaderEventColorG, Settings::nodeHeaderEventColorB, Settings::nodeHeaderEventColorA);
        case NodeColorCategory::Data:
            return ImVec4(Settings::nodeHeaderDataColorR, Settings::nodeHeaderDataColorG, Settings::nodeHeaderDataColorB, Settings::nodeHeaderDataColorA);
        case NodeColorCategory::Struct:
            return ImVec4(Settings::nodeHeaderStructColorR, Settings::nodeHeaderStructColorG, Settings::nodeHeaderStructColorB, Settings::nodeHeaderStructColorA);
        case NodeColorCategory::Logic:
            return ImVec4(Settings::nodeHeaderLogicColorR, Settings::nodeHeaderLogicColorG, Settings::nodeHeaderLogicColorB, Settings::nodeHeaderLogicColorA);
        case NodeColorCategory::Flow:
            return ImVec4(Settings::nodeHeaderFlowColorR, Settings::nodeHeaderFlowColorG, Settings::nodeHeaderFlowColorB, Settings::nodeHeaderFlowColorA);
        case NodeColorCategory::More:
        default:
            return ImVec4(Settings::nodeHeaderMoreColorR, Settings::nodeHeaderMoreColorG, Settings::nodeHeaderMoreColorB, Settings::nodeHeaderMoreColorA);
    }
}
}

void NodePreview::Draw(NodeType nodeType, const char* titleOverride)
{
    const NodeRegistry& registry = NodeRegistry::Get();
    const NodeDescriptor* desc = registry.Find(nodeType);

    // Layout constants
    const float padding      = nodePreviewConstants::padding;
    const float headerHeight = nodePreviewConstants::headerHeight;
    const float fixedWidth   = nodePreviewConstants::fixedWidth;
    const float fixedHeight  = nodePreviewConstants::fixedHeight;
    const float pinRadius    = nodePreviewConstants::pinRadius;

    // Separate input and output pins (with small variant-specific filtering)
    std::vector<const PinDescriptor*> inputPins;
    std::vector<const PinDescriptor*> outputPins;
    std::vector<const FieldDescriptor*> visibleFields;

    const std::string override = titleOverride ? titleOverride : "";
    const bool isVariableSetPreview = (nodeType == NodeType::Variable && override == "Set Variable");
    const bool isVariableGetPreview = (nodeType == NodeType::Variable && override == "Get Variable");
    const bool isDrawRectPreview = (nodeType == NodeType::DrawRect);
    const bool isDrawGridPreview = (nodeType == NodeType::DrawGrid);
    const bool isStructDefinePreview = (nodeType == NodeType::StructDefine);
    const bool isStructCreatePreview = (nodeType == NodeType::StructCreate);
    const bool isStructGetPreview = (nodeType == NodeType::StructGetField);
    const bool isStructSetPreview = (nodeType == NodeType::StructSetField);
    const bool isStructDeletePreview = (nodeType == NodeType::StructDelete);

    auto keepPin = [&](const PinDescriptor& pd) -> bool
    {
        if (isDrawRectPreview)
            return (pd.name == "In" || pd.name == "X" || pd.name == "Y" || pd.name == "Out");

        if (isDrawGridPreview)
            return (pd.name == "In" || pd.name == "W" || pd.name == "H" || pd.name == "Out");

        if (isStructDefinePreview)
            return (!pd.isInput && pd.name == "Schema");

        if (isStructCreatePreview)
            return (pd.name == "Struct" || pd.name == "Item");

        if (isStructGetPreview)
            return (pd.name == "Item" || pd.name == "Value");

        if (isStructSetPreview)
            return (pd.name == "Item" || pd.name == "Value" || pd.name == "Out");

        if (isStructDeletePreview)
            return (pd.name == "In" || pd.name == "Array" || pd.name == "Out");

        if (!isVariableSetPreview && !isVariableGetPreview)
            return true;

        if (isVariableGetPreview)
            return (!pd.isInput && pd.name == "Value");

        // Set Variable preview: keep execution + default/value pins only.
        return (pd.name == "In" || pd.name == "Default" || pd.name == "Out" || pd.name == "Value");
    };

    auto keepField = [&](const FieldDescriptor& fd) -> bool
    {
        if (fd.name == "Variant")
            return false; // internal only

        if (isVariableGetPreview)
            return (fd.name == "Name");

        if (isVariableSetPreview)
            return (fd.name == "Name" || fd.name == "Default");

        if (isDrawRectPreview)
            return (fd.name == "X" || fd.name == "Y" || fd.name == "W" || fd.name == "H");

        if (isDrawGridPreview)
            return (fd.name == "W" || fd.name == "H");

        if (isStructDefinePreview)
            return (fd.name == "Struct Name" || fd.name == "Fields");

        if (isStructCreatePreview)
            return (fd.name == "Struct Name");

        if (isStructGetPreview || isStructSetPreview)
            return (fd.name == "Struct Name" || fd.name == "Field");

        if (isStructDeletePreview)
            return (fd.name == "Id");

        return true;
    };

    for (const PinDescriptor& pd : desc->pins)
    {
        if (!keepPin(pd))
            continue;

        if (pd.isInput)
            inputPins.push_back(&pd);
        else
            outputPins.push_back(&pd);
    }

    for (const FieldDescriptor& fd : desc->fields)
    {
        if (keepField(fd))
            visibleFields.push_back(&fd);
    }

    // Dynamic pin spacing — more items means less space between them
    const float totalItems         = static_cast<float>(inputPins.size() + outputPins.size() + visibleFields.size());
    const float contentHeightAvail = fixedHeight - headerHeight - 2.0f * padding;
    const float pinSpacing         = totalItems > 1.0f ? contentHeightAvail / totalItems : contentHeightAvail;

    // Positioning
    const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImDrawList*  drawList  = ImGui::GetWindowDrawList();

    const ImU32 bgColor     = ImGui::GetColorU32(colors::surface);
    const bool isInactivePreview = ImGui::GetStyle().Alpha < 0.99f;
    const ImU32 textColor   = ImGui::GetColorU32(isInactivePreview ? colors::textSecondary : colors::textPrimary);
    const ImU32 pinColor    = ImGui::GetColorU32(colors::accent);
    const ImU32 headerColor = ImGui::GetColorU32(GetNodeCategoryHeaderColor(nodeType));
    const ImU32 borderColorNormal = headerColor;
    const ImU32 borderColorHover = ImGui::GetColorU32(colors::accent);

    const ImVec2 nodeMax(cursorPos.x + fixedWidth, cursorPos.y + fixedHeight);

    // Hover effect
    const bool  hovered         = ImGui::IsMouseHoveringRect(cursorPos, nodeMax);
    const ImU32 borderColor     = hovered ? borderColorHover : borderColorNormal;
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
    const char* titleText = (titleOverride && titleOverride[0] != '\0')
        ? titleOverride
        : desc->label.c_str();
    drawList->AddText(ImVec2(cursorPos.x + padding, cursorPos.y + 3.0f), textColor, titleText);

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
    for (const FieldDescriptor* field : visibleFields)
    {
        const float fieldY = contentTop + verticalOffset + static_cast<float>(idx) * pinSpacing;
        const char* label = field->name.c_str();
        if (isVariableGetPreview && field->name == "Name")
            label = "Variable";
        drawList->AddText(ImVec2(cursorPos.x + padding + 8.0f, fieldY - 6.0f), textColor, label);
        ++idx;
    }

    // Output pins
    // Output pins — text right-aligned, circle on the right edge
    for (const PinDescriptor* pin : outputPins)
    {
        const float  pinY   = contentTop + verticalOffset + static_cast<float>(idx) * pinSpacing;
        const ImVec2 pinPos(nodeMax.x - padding, pinY); // circle on right edge
        drawList->AddCircleFilled(pinPos, pinRadius, pinColor);
        snprintf(buf, sizeof(buf), "%s ->", pin->name.c_str());
        const float textWidth = ImGui::CalcTextSize(buf).x;
        drawList->AddText(ImVec2(pinPos.x - pinRadius - 4.0f - textWidth, pinY - 6.0f), textColor, buf);
        ++idx;
    }

    ImGui::PopFont();

    ImGui::Dummy(ImVec2(fixedWidth, fixedHeight));
}