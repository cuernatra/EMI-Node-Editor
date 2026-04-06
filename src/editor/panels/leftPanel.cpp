#include "leftPanel.h"
#include "core/registry/nodeRegistry.h"
#include "editor/spawnPayload.h"
#include "editor/nodeColorCategories.h"
#include "editor/settings.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace
{
struct PaletteItem
{
    NodeType type = NodeType::Unknown;
    std::string displayLabel;
    std::string payloadTitle;
    bool disabled = false;
};

std::string ToLowerCopy(const std::string& value)
{
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

bool MatchesSearch(const std::string& searchText, const std::string& candidate)
{
    if (searchText.empty())
        return true;

    return ToLowerCopy(candidate).find(ToLowerCopy(searchText)) != std::string::npos;
}

ImVec4 GetNodeCategoryHeaderColor(const NodeDescriptor* desc)
{
    switch (GetNodeColorCategory(desc))
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

void DrawNodePreview(NodeType nodeType, const char* titleOverride)
{
    const NodeRegistry& registry = NodeRegistry::Get();
    const NodeDescriptor* desc = registry.Find(nodeType);
    if (!desc)
        return;

    const float padding = nodePreviewConstants::padding;
    const float headerHeight = nodePreviewConstants::headerHeight;
    const float fixedWidth = nodePreviewConstants::fixedWidth;
    const float fixedHeight = nodePreviewConstants::fixedHeight;
    const float pinRadius = nodePreviewConstants::pinRadius;

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

        if (!isVariableSetPreview && !isVariableGetPreview)
            return true;

        if (isVariableGetPreview)
            return (!pd.isInput && pd.name == "Value");

        return (pd.name == "In" || pd.name == "Default" || pd.name == "Out" || pd.name == "Value");
    };

    auto keepField = [&](const FieldDescriptor& fd) -> bool
    {
        if (fd.name == "Variant")
            return false;

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

    const float totalItems = static_cast<float>(inputPins.size() + outputPins.size() + visibleFields.size());
    const float contentHeightAvail = fixedHeight - headerHeight - 2.0f * padding;
    const float pinSpacing = totalItems > 1.0f ? contentHeightAvail / totalItems : contentHeightAvail;

    const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    const ImU32 bgColor = ImGui::GetColorU32(colors::surface);
    const bool isInactivePreview = ImGui::GetStyle().Alpha < 0.99f;
    const ImU32 textColor = ImGui::GetColorU32(isInactivePreview ? colors::textSecondary : colors::textPrimary);
    const ImU32 pinColor = ImGui::GetColorU32(colors::accent);
    const ImU32 headerColor = ImGui::GetColorU32(GetNodeCategoryHeaderColor(desc));
    const ImU32 borderColorNormal = headerColor;
    const ImU32 borderColorHover = ImGui::GetColorU32(colors::accent);

    const ImVec2 nodeMax(cursorPos.x + fixedWidth, cursorPos.y + fixedHeight);
    const bool hovered = ImGui::IsMouseHoveringRect(cursorPos, nodeMax);
    const ImU32 borderColor = hovered ? borderColorHover : borderColorNormal;
    const float expand = hovered ? 1.0f : 0.0f;
    const float borderThickness = 1.0f + 2.0f * expand;

    drawList->AddRectFilled(cursorPos, nodeMax, bgColor, 2.0f);
    drawList->AddRect(
        ImVec2(cursorPos.x - expand, cursorPos.y - expand),
        ImVec2(nodeMax.x + expand, nodeMax.y + expand),
        borderColor, 2.0f, 0, borderThickness
    );

    drawList->AddRectFilled(
        cursorPos,
        ImVec2(cursorPos.x + fixedWidth, cursorPos.y + headerHeight),
        headerColor,
        2.0f
    );
    const char* titleText = (titleOverride && titleOverride[0] != '\0') ? titleOverride : desc->label.c_str();
    drawList->AddText(ImVec2(cursorPos.x + padding, cursorPos.y + 3.0f), textColor, titleText);

    const float contentTop = cursorPos.y + headerHeight + padding;
    const float totalItemsHeight = (totalItems - 1.0f) * pinSpacing;
    float verticalOffset = (contentHeightAvail - totalItemsHeight) / 2.0f;
    verticalOffset = std::max(verticalOffset, pinSpacing * 0.5f);

    ImGui::PushFont(nullptr);

    char buf[128];
    size_t idx = 0;

    for (const PinDescriptor* pin : inputPins)
    {
        const float pinY = contentTop + verticalOffset + static_cast<float>(idx) * pinSpacing;
        const ImVec2 pinPos(cursorPos.x + padding, pinY);
        drawList->AddCircleFilled(pinPos, pinRadius, pinColor);
        snprintf(buf, sizeof(buf), "-> %s", pin->name.c_str());
        drawList->AddText(ImVec2(pinPos.x + pinRadius + 4.0f, pinY - 6.0f), textColor, buf);
        ++idx;
    }

    for (const FieldDescriptor* field : visibleFields)
    {
        const float fieldY = contentTop + verticalOffset + static_cast<float>(idx) * pinSpacing;
        const char* label = field->name.c_str();
        if (isVariableGetPreview && field->name == "Name")
            label = "Variable";
        drawList->AddText(ImVec2(cursorPos.x + padding + 8.0f, fieldY - 6.0f), textColor, label);
        ++idx;
    }

    for (const PinDescriptor* pin : outputPins)
    {
        const float pinY = contentTop + verticalOffset + static_cast<float>(idx) * pinSpacing;
        const ImVec2 pinPos(nodeMax.x - padding, pinY);
        drawList->AddCircleFilled(pinPos, pinRadius, pinColor);
        snprintf(buf, sizeof(buf), "%s ->", pin->name.c_str());
        const float textWidth = ImGui::CalcTextSize(buf).x;
        drawList->AddText(ImVec2(pinPos.x - pinRadius - 4.0f - textWidth, pinY - 6.0f), textColor, buf);
        ++idx;
    }

    ImGui::PopFont();
    ImGui::Dummy(ImVec2(fixedWidth, fixedHeight));
}

void DrawNodeItem(const PaletteItem& item)
{
    const NodeDescriptor* descriptor = NodeRegistry::Get().Find(item.type);
    const char* payloadTitle = item.payloadTitle.empty()
        ? (descriptor ? descriptor->saveToken.c_str() : "Unknown")
        : item.payloadTitle.c_str();

    ImGui::PushID(payloadTitle);

    if (item.disabled)
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);

    const char* previewTitle = item.displayLabel.empty() ? nullptr : item.displayLabel.c_str();
    DrawNodePreview(item.type, previewTitle);

    if (!item.disabled && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
        NodeSpawnPayload payload{};
        std::strncpy(payload.title, payloadTitle, sizeof(payload.title) - 1);
        ImGui::SetDragDropPayload("SPAWN_NODE", &payload, sizeof(NodeSpawnPayload));
        DrawNodePreview(item.type, previewTitle);
        ImGui::EndDragDropSource();
    }

    if (item.disabled)
        ImGui::PopStyleVar();

    ImGui::PopID();
}

void DrawSection(const char* title, const std::vector<PaletteItem>& items)
{
    if (ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen))
    {
        const float leftBarWidth = ImGui::GetContentRegionAvail().x;
        
        // draw nodes on the same line until we run out of horizontal space, then wrap to next line
        size_t col = 0;
        size_t idx = 0;
        for (const PaletteItem& item : items)
        {
            DrawNodeItem(item);
            col++;
            idx++;
            if (idx < items.size()
                && (nodePreviewConstants::fixedWidth + nodePreviewConstants::padding)
                * (col + 1) < leftBarWidth)
                    ImGui::SameLine();
            else
                col = 0;
        }
    }
}
}

LeftPanel::LeftPanel()
{
    // Keep palette deterministic by sorting all registered nodes by label.
    const auto& registry = NodeRegistry::Get();
    const auto& allDescriptors = registry.All();
    for (const auto& kv : allDescriptors)
        m_nodeTypes.push_back(kv.first);

    std::sort(m_nodeTypes.begin(), m_nodeTypes.end(), [&](NodeType a, NodeType b) {
        const NodeDescriptor* da = registry.Find(a);
        const NodeDescriptor* db = registry.Find(b);
        const std::string& la = da ? da->label : std::string();
        const std::string& lb = db ? db->label : std::string();
        if (la != lb)
            return la < lb;
        return static_cast<int>(a) < static_cast<int>(b);
    });
}

void LeftPanel::draw(bool hasStartNode)
{
    ImGui::Text("NODE PALETTE");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputTextWithHint("##nodePaletteSearch", "Search nodes...", m_searchBuffer, sizeof(m_searchBuffer));
    ImGui::Separator();

    const std::string searchText = m_searchBuffer;
    std::map<std::string, std::vector<PaletteItem>> sections;
    const auto& registry = NodeRegistry::Get();

    auto makeDefaultItem = [&](NodeType t) -> PaletteItem
    {
        PaletteItem item;
        item.type = t;
        if (const NodeDescriptor* desc = registry.Find(t))
            item.payloadTitle = desc->saveToken;
        item.disabled = (t == NodeType::Start && hasStartNode);
        return item;
    };

    for (NodeType t : m_nodeTypes)
    {
        const NodeDescriptor* desc = registry.Find(t);
        if (!desc)
            continue;

        const std::string category = desc->category.empty() ? "More" : desc->category;

        if (!desc->paletteVariants.empty())
        {
            for (const PaletteVariant& variant : desc->paletteVariants)
            {
                PaletteItem item;
                item.type = t;
                item.displayLabel = variant.displayLabel;
                item.payloadTitle = variant.payloadTitle;
                item.disabled = (t == NodeType::Start && hasStartNode);

                const std::string searchCandidate = item.displayLabel.empty()
                    ? desc->label + " " + item.payloadTitle + " " + desc->saveToken
                    : item.displayLabel + " " + item.payloadTitle + " " + desc->label + " " + desc->saveToken;
                if (MatchesSearch(searchText, searchCandidate))
                    sections[category].push_back(item);
            }
        }
        else
        {
            PaletteItem item = makeDefaultItem(t);
            const std::string searchCandidate = desc->label + " " + desc->saveToken;
            if (MatchesSearch(searchText, searchCandidate))
                sections[category].push_back(item);
        }
    }

    bool hasAnyResults = false;
    for (const auto& [category, items] : sections)
    {
        if (items.empty())
            continue;

        hasAnyResults = true;
        DrawSection(category.c_str(), items);
    }

    if (!hasAnyResults)
        ImGui::TextDisabled("No matching nodes.");
}
