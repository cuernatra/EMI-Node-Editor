#include "dropBar.h"
#include "visualNode.h"
#include <cstring>

DropBar::DropBar(std::string name, int id, float width, float height, bool hasSpawnButton)
    : m_name{std::move(name)}
    , m_id{id}
    , m_width{width}
    , m_height{height}
    , m_open{false}
    , m_hasSpawnButton{hasSpawnButton}
{
}

void DropBar::draw()
{
    ImGui::PushID(m_id);

    m_open = ImGui::CollapsingHeader(m_name.c_str());

    if (m_open)
    {
        ImGui::Text("ID: %d", m_id);

        if (m_hasSpawnButton)
        {
            if (ImGui::Button("Create Node", ImVec2(m_width * 3/2, 0)))
            {
            }

            if (ImGui::BeginDragDropSource())
            {
                const char* title = m_name.c_str();

                ImGui::SetDragDropPayload(
                    "SPAWN_TEST_NODE",
                    title,
                    (int)strlen(title) + 1
                );

                ImGui::Text("Create: %s", title);
                ImGui::EndDragDropSource();
            }
        }
    }

    ImGui::PopID();
}
