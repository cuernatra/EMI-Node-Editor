#include "consolePanel.h"
#include <imgui.h>
#include <cstdarg>

ConsolePanel::ConsolePanel() : m_height{300} // default height for console panel
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
    return m_height;
}

void ConsolePanel::draw()
{
    ImGui::BeginChild("CONSOLE", ImVec2(0, m_height), true);
    if (ImGui::Button("Clear"))
    {
        clear();
    }
    ImGui::Separator();
    ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
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