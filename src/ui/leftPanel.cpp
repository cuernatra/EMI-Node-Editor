#include "leftPanel.h"
#include "../core/registry/nodeRegistry.h"
#include "../core/graph/link.h"
#include "imgui.h"
#include <cstring>
#include "nodePreview.h"

namespace
{
void DrawNodeItem(NodeType nodeType)
{
    const char* label = NodeTypeToString(nodeType);
    NodePreview::Draw(nodeType);
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
        NodeSpawnPayload payload{};
        std::strncpy(payload.title, label, sizeof(payload.title) - 1);
        ImGui::SetDragDropPayload("SPAWN_NODE", &payload, sizeof(NodeSpawnPayload));
        NodePreview::Draw(nodeType);
        ImGui::EndDragDropSource();
    }
}

void DrawSection(const char* title, const std::vector<NodeType>& types)
{
    if (ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen))
    {
        const float leftBarWidth = ImGui::GetContentRegionAvail().x;
        
        // draw nodes on the same line until we run out of horizontal space, then wrap to next line
        size_t col = 0;
        size_t idx = 0;
        for (NodeType t : types)
        {
            DrawNodeItem(t);
            col++;
            idx++;
            if (idx < types.size()
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
        NodeType::Constant,
        NodeType::Variable,
        NodeType::Operator,
        NodeType::Comparison,
        NodeType::Logic,
        NodeType::Function,
        NodeType::FunctionCall,
        NodeType::Sequence,
        NodeType::Branch,
        NodeType::Loop,
        NodeType::Output
    };

    for (NodeType t : preferredOrder)
        if (allDescriptors.find(t) != allDescriptors.end())
            m_nodeTypes.push_back(t);
}

void LeftPanel::draw()
{
    ImGui::Text("NODE PALETTE");
    ImGui::Separator();

    std::vector<NodeType> dataTypes;
    std::vector<NodeType> logicTypes;
    std::vector<NodeType> flowTypes;

    for (NodeType t : m_nodeTypes)
    {
        switch (t)
        {
            case NodeType::Constant:
            case NodeType::Variable:
                dataTypes.push_back(t);
                break;

            case NodeType::Operator:
            case NodeType::Comparison:
            case NodeType::Logic:
            case NodeType::Function:
                logicTypes.push_back(t);
            case NodeType::FunctionCall:
                logicTypes.push_back(t);
                break;

            case NodeType::Sequence:
            case NodeType::Branch:
            case NodeType::Loop:
            case NodeType::Start:
            case NodeType::Output:
                flowTypes.push_back(t);
                break;

            default:
                logicTypes.push_back(t);
                break;
        }
    }

    DrawSection("Data", dataTypes);
    DrawSection("Logic", logicTypes);
    DrawSection("Flow", flowTypes);
}
