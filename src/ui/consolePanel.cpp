#include "consolePanel.h"
#include <imgui.h>
#include <cstdarg>
#include <ctime>

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

    char timeBuf[16] = {0};
    std::time_t now = std::time(nullptr);
    std::tm localTime{};
    if (localtime_s(&localTime, &now) == 0)
    {
        std::strftime(timeBuf, IM_ARRAYSIZE(timeBuf), "[%H:%M:%S] ", &localTime);
    }

    if (timeBuf[0] != 0)
    {
        m_logs.push_back(std::string(timeBuf) + buf);
    }
    else
    {
        m_logs.push_back(buf);
    }
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

    const char* toggleLabel = m_minimized ? "Maximize" : "Minimize";
    const float framePadX = ImGui::GetStyle().FramePadding.x;
    const float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
    const float toggleWidth = ImGui::CalcTextSize(toggleLabel).x + framePadX * 2.0f;
    const float clearWidth = ImGui::CalcTextSize("Clear").x + framePadX * 2.0f;
    const float buttonsWidth = m_minimized ? toggleWidth : (clearWidth + itemSpacing + toggleWidth);

    const float currentX = ImGui::GetCursorPosX();
    const float rightAlignedX = currentX + ImGui::GetContentRegionAvail().x - buttonsWidth;
    ImGui::SetCursorPosX(std::max(currentX, rightAlignedX));

    if (m_minimized)
    {
        if (ImGui::Button(toggleLabel))
        {
            toggleMinimized();
        }
        ImGui::EndChild();
        return;
    }

    if (ImGui::Button("Clear"))
    {
        clear();
    }

    ImGui::SameLine();
    if (ImGui::Button(toggleLabel))
    {
        toggleMinimized();
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