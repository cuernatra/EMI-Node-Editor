#include "dropBar.h"

DropBar::DropBar(std::string name, int id, float width, float height) 
    : m_name{std::move(name)}
    , m_id{id}
    , m_width{width}
    , m_height{height}
    , m_open{false}
{
}

void DropBar::draw()
{   
    ImGui::PushID(m_id);

    m_open = ImGui::CollapsingHeader(m_name.c_str());

    if (m_open)
    {
        ImGui::Text("ID: %d", m_id);
    }

    ImGui::PopID();
}