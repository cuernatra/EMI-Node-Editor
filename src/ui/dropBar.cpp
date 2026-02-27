#include "dropBar.h"
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
            // The drag source must be activated while the button is the
            // last active item, so BeginDragDropSource goes immediately
            // after the button — before any other widget.
            ImGui::Button("Drag to create", ImVec2(m_width * 1.5f, 0));

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

                ImGui::Text("Create: %s", m_label.c_str());
                ImGui::EndDragDropSource();
            }
        }
    }

    ImGui::PopID();
}
