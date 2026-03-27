#include "leftPanel.h"
#include "../core/registry/nodeRegistry.h"
#include "../core/graph/link.h"
#include "imgui.h"
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include "nodePreview.h"

namespace
{
struct PaletteItem
{
    NodeType type = NodeType::Unknown;
    std::string displayLabel;
    std::string payloadTitle;
    bool disabled = false;
};

void DrawNodeItem(const PaletteItem& item)
{
    const char* payloadTitle = item.payloadTitle.empty()
        ? NodeTypeToString(item.type)
        : item.payloadTitle.c_str();

    ImGui::PushID(payloadTitle);

    if (item.disabled)
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);

    const char* previewTitle = item.displayLabel.empty() ? nullptr : item.displayLabel.c_str();
    NodePreview::Draw(item.type, previewTitle);

    if (!item.disabled && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
        NodeSpawnPayload payload{};
        std::strncpy(payload.title, payloadTitle, sizeof(payload.title) - 1);
        ImGui::SetDragDropPayload("SPAWN_NODE", &payload, sizeof(NodeSpawnPayload));
        NodePreview::Draw(item.type, previewTitle);
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
    // Keep palette deterministic and grouped in UI.
    const auto& registry = NodeRegistry::Get();
    const auto& allDescriptors = registry.All();

    const NodeType preferredOrder[] = {
        NodeType::Start,
        NodeType::Constant,
        NodeType::Variable,
        NodeType::Operator,
        NodeType::Comparison,
        NodeType::Logic,
        NodeType::Function,
        NodeType::Sequence,
        NodeType::Branch,
        NodeType::Loop,
        NodeType::While,
        NodeType::Output
    };

    for (NodeType t : preferredOrder)
        if (allDescriptors.find(t) != allDescriptors.end())
            m_nodeTypes.push_back(t);
}

void LeftPanel::draw(bool hasStartNode)
{
    ImGui::Text("NODE PALETTE");
    ImGui::Separator();

    std::vector<PaletteItem> eventTypes;
    std::vector<PaletteItem> dataTypes;
    std::vector<PaletteItem> logicTypes;
    std::vector<PaletteItem> flowTypes;

    auto makeDefaultItem = [&](NodeType t) -> PaletteItem
    {
        PaletteItem item;
        item.type = t;
        item.payloadTitle = NodeTypeToString(t);
        item.disabled = (t == NodeType::Start && hasStartNode);
        return item;
    };

    for (NodeType t : m_nodeTypes)
    {
        switch (t)
        {
            case NodeType::Start:
                eventTypes.push_back(makeDefaultItem(t));
                break;

            case NodeType::Constant:
                dataTypes.push_back(makeDefaultItem(t));
                break;

            case NodeType::Variable:
            {
                PaletteItem setItem;
                setItem.type = NodeType::Variable;
                setItem.displayLabel = "Set Variable";
                setItem.payloadTitle = "Variable:Set";

                PaletteItem getItem;
                getItem.type = NodeType::Variable;
                getItem.displayLabel = "Get Variable";
                getItem.payloadTitle = "Variable:Get";

                dataTypes.push_back(setItem);
                dataTypes.push_back(getItem);
                break;
            }

            case NodeType::Operator:
            case NodeType::Comparison:
            case NodeType::Logic:
            case NodeType::Function:
                logicTypes.push_back(makeDefaultItem(t));
                break;

            case NodeType::Sequence:
            case NodeType::Branch:
            case NodeType::Loop:
            case NodeType::While:
            case NodeType::Output:
                flowTypes.push_back(makeDefaultItem(t));
                break;

            default:
                logicTypes.push_back(makeDefaultItem(t));
                break;
        }
    }

    DrawSection("Events", eventTypes);
    DrawSection("Data", dataTypes);
    DrawSection("Logic", logicTypes);
    DrawSection("Flow", flowTypes);
}
