#include "consolePanel.h"
#include <imgui.h>
#include <cstdarg>

ConsolePanel::ConsolePanel()
    : m_height{300}, m_lastExpandedHeight{300}, m_minimized{false}
{
}

void ConsolePanel::addLog(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
    buf[IM_ARRAYSIZE(buf)-1] = 0;
    va_end(args);

    m_logs.push_back(buf);
}

void ConsolePanel::clear()
{
    m_logs.clear();
}

float ConsolePanel::getHeight() const
{
    return m_minimized ? 32.0f : m_height;
}

void ConsolePanel::setHeight(float height)
{
    if (!m_minimized)
    {
        m_height = height;
        m_lastExpandedHeight = height;
    }
}

void ConsolePanel::toggleMinimized()
{
    if (m_minimized)
    {
        m_minimized = false;
        m_height = m_lastExpandedHeight;
    }
    else
    {
        m_lastExpandedHeight = m_height;
        m_minimized = true;
    }
}

bool ConsolePanel::isMinimized() const
{
    return m_minimized;
}

void ConsolePanel::draw()
{
    const ImGuiWindowFlags consoleFlags = m_minimized
        ? (ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
        : ImGuiWindowFlags_None;
    ImGui::BeginChild("CONSOLE", ImVec2(0, getHeight()), true, consoleFlags);

    if (ImGui::Button(m_minimized ? "Maximize" : "Minimize"))
    {
        toggleMinimized();
    }

    if (m_minimized)
    {
        ImGui::EndChild();
        return;
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        clear();
    }

    ImGui::Separator();
    ImGui::BeginChild("scrolling", ImVec2(0, 0), false);
    
    for (const auto& log : m_logs)
    {
        ImGui::TextUnformatted(log.c_str());
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::EndChild();
}