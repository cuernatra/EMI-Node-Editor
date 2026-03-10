#include "dropBar.h"
#include "nodePreview.h"
#include "imgui.h"
#include <cstring>

DropBar::DropBar(std::string label, int id, NodeType nodeType,
                 float width, float height, bool hasSpawnButton)
    : m_label{std::move(label)}
    , m_id{id}
    , m_nodeType{nodeType}
    , m_width{width}
    , m_height{height}
    , m_open{false}
    , m_hasSpawnButton{hasSpawnButton}
{
}

void DropBar::draw()
{
    ImGui::PushID(m_id);

    m_open = ImGui::CollapsingHeader(m_label.c_str());

    if (m_open)
    {
        if (m_hasSpawnButton)
        {
            // Draw the node preview
            NodePreview::Draw(m_nodeType, m_width * 1.5f, m_height);

            // The drag source must be activated immediately after drawing the preview
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                NodeSpawnPayload payload{};
                std::strncpy(payload.title,
                             NodeTypeToString(m_nodeType),
                             sizeof(payload.title) - 1);
                // payload.pinType stays Any unless you want context-aware wiring

                ImGui::SetDragDropPayload(
                    "SPAWN_NODE",
                    &payload,
                    sizeof(NodeSpawnPayload)
                );

                // Show preview while dragging
                NodePreview::Draw(m_nodeType, m_width * 1.5f, m_height);
                ImGui::EndDragDropSource();
            }
        }
    }

    ImGui::PopID();
}
