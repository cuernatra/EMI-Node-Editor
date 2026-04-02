#include "leftPanel.h"
#include "core/registry/nodeRegistry.h"
#include "editor/spawnPayload.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>
#include "editor/widgets/nodePreview.h"

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

void DrawStructSubSection(const char* title, const std::vector<PaletteItem>& items)
{
    if (items.empty())
        return;

    if (ImGui::TreeNodeEx(title, ImGuiTreeNodeFlags_DefaultOpen))
    {
        const float leftBarWidth = ImGui::GetContentRegionAvail().x;

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

        ImGui::TreePop();
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
        NodeType::StructDefine,
        NodeType::StructCreate,
        NodeType::StructGetField,
        NodeType::StructSetField,
        NodeType::ArrayGetAt,
        NodeType::Operator,
        NodeType::Comparison,
        NodeType::Logic,
        NodeType::Not,
        NodeType::DrawRect,
        NodeType::DrawGrid,
        NodeType::Function,
        NodeType::Delay,
        NodeType::Sequence,
        NodeType::Branch,
        NodeType::Loop,
        NodeType::ForEach,
        NodeType::StructDelete,
        NodeType::ArrayAddAt,
        NodeType::ArrayRemoveAt,
        NodeType::While,
        NodeType::Output
    };

    std::unordered_set<NodeType> added;
    added.reserve(allDescriptors.size());

    for (NodeType t : preferredOrder)
    {
        if (allDescriptors.find(t) != allDescriptors.end())
        {
            m_nodeTypes.push_back(t);
            added.insert(t);
        }
    }

    // Append any other registered node types not explicitly ordered above.
    // These will land in the "More" section unless later classified.
    std::vector<NodeType> remaining;
    remaining.reserve(allDescriptors.size());
    for (const auto& kv : allDescriptors)
    {
        const NodeType t = kv.first;
        if (added.find(t) == added.end())
            remaining.push_back(t);
    }

    std::sort(remaining.begin(), remaining.end(), [&](NodeType a, NodeType b) {
        const NodeDescriptor* da = registry.Find(a);
        const NodeDescriptor* db = registry.Find(b);
        const std::string& la = da ? da->label : std::string();
        const std::string& lb = db ? db->label : std::string();
        if (la != lb)
            return la < lb;
        return static_cast<int>(a) < static_cast<int>(b);
    });

    m_nodeTypes.insert(m_nodeTypes.end(), remaining.begin(), remaining.end());
}

void LeftPanel::draw(bool hasStartNode)
{
    ImGui::Text("NODE PALETTE");
    ImGui::Separator();

    std::vector<PaletteItem> eventTypes;
    std::vector<PaletteItem> dataTypes;
    std::vector<PaletteItem> structSchemaTypes;
    std::vector<PaletteItem> structValueTypes;
    std::vector<PaletteItem> structFlowTypes;
    std::vector<PaletteItem> logicTypes;
    std::vector<PaletteItem> flowTypes;
    std::vector<PaletteItem> moreTypes;

    auto makeDefaultItem = [&](NodeType t) -> PaletteItem
    {
        PaletteItem item;
        item.type = t;
        item.payloadTitle = NodeTypeToSaveToken(t);
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
            case NodeType::ArrayGetAt:
                dataTypes.push_back(makeDefaultItem(t));
                break;

            case NodeType::StructDefine:
                structSchemaTypes.push_back(makeDefaultItem(t));
                break;

            case NodeType::StructCreate:
            case NodeType::StructGetField:
            case NodeType::StructSetField:
                structValueTypes.push_back(makeDefaultItem(t));
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
            case NodeType::Not:
            case NodeType::DrawGrid:
            case NodeType::Function:
                logicTypes.push_back(makeDefaultItem(t));
                break;

            case NodeType::Delay:
            case NodeType::DrawRect:
            case NodeType::Sequence:
            case NodeType::Branch:
            case NodeType::Loop:
            case NodeType::ForEach:
            case NodeType::ArrayAddAt:
            case NodeType::ArrayRemoveAt:
            case NodeType::While:
            case NodeType::Output:
                flowTypes.push_back(makeDefaultItem(t));
                break;

            case NodeType::StructDelete:
                structFlowTypes.push_back(makeDefaultItem(t));
                break;

            default:
                moreTypes.push_back(makeDefaultItem(t));
                break;
        }
    }

    DrawSection("Events", eventTypes);
    DrawSection("Data", dataTypes);

    if (ImGui::CollapsingHeader("Structs", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawStructSubSection("Schema", structSchemaTypes);
        DrawStructSubSection("Values", structValueTypes);
        DrawStructSubSection("Flow", structFlowTypes);
    }

    DrawSection("Logic", logicTypes);
    DrawSection("Flow", flowTypes);
    DrawSection("More", moreTypes);
}
