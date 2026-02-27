#include "leftPanel.h"
#include "../core/registry/nodeRegistry.h"

LeftPanel::LeftPanel()
{
    float w = elementSizes::dropBarWidth;
    float h = elementSizes::dropBarHeight;

    // Auto-populate from registry
    const auto& registry = NodeRegistry::Get();
    const auto& allDescriptors = registry.All();
    
    int id = 0;
    for (const auto& [nodeType, descriptor] : allDescriptors)
    {
        if (nodeType == NodeType::Unknown)
            continue;
        
        const char* label = NodeTypeToString(nodeType);
        m_dropBars.emplace_back(label, id++, nodeType, w, h, true);
    }
}

void LeftPanel::draw()
{
    ImGui::Text("NODE PALETTE");
    ImGui::Separator();

    for (auto& bar : m_dropBars)
        bar.draw();
}
