#include "leftPanel.h"
#include "core/registry/nodeRegistry.h"
#include "editor/spawnPayload.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <map>
#include <string>
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
    const NodeDescriptor* descriptor = NodeRegistry::Get().Find(item.type);
    const char* payloadTitle = item.payloadTitle.empty()
        ? (descriptor ? descriptor->saveToken.c_str() : "Unknown")
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
    // Kept for compatibility with older call sites; currently unused.
    DrawSection(title, items);
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
    ImGui::Separator();

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
                sections[category].push_back(item);
            }
        }
        else
        {
            sections[category].push_back(makeDefaultItem(t));
        }
    }

    for (const auto& [category, items] : sections)
        DrawSection(category.c_str(), items);
}
