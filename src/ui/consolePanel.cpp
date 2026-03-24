#include "consolePanel.h"
#include <imgui.h>
#include <cstdarg>
#include <ctime>
#include "../editor/settings.h"

std::string ConsolePanel::makeTimestampPrefix() const
{
    char timeBuf[16] = {0};
    std::time_t now = std::time(nullptr);
    std::tm localTime{};

    bool hasLocalTime = false;
#ifdef _WIN32
    hasLocalTime = (localtime_s(&localTime, &now) == 0);
#else
    hasLocalTime = (localtime_r(&now, &localTime) != nullptr);
#endif

    if (hasLocalTime)
    {
        std::strftime(timeBuf, IM_ARRAYSIZE(timeBuf), "[%H:%M:%S] ", &localTime);
    }

    if (timeBuf[0] != 0)
    {
        return std::string(timeBuf);
    }
    return std::string();
}

std::string ConsolePanel::withTimestamp(const std::string& message) const
{
    return makeTimestampPrefix() + message;
}

void ConsolePanel::pushCappedLine(const std::string& line)
{
    m_logs.push_back(line);
    while (m_logs.size() > kMaxLogLines)
    {
        m_logs.pop_front();
    }
}

ConsolePanel::ConsolePanel()
    : m_height{Settings::consolePanelHeight}, m_lastExpandedHeight{Settings::consolePanelHeight}, m_minimized{Settings::consolePanelIsMinimized}
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

    addLogText(std::string(buf) + "\n");
}

void ConsolePanel::addLogText(const std::string& message)
{
    std::lock_guard<std::mutex> lock(m_logsMutex);

    for (char ch : message)
    {
        if (ch == '\r')
        {
            if (m_activeLinePrefix.empty())
            {
                m_activeLinePrefix = makeTimestampPrefix();
            }
            m_activeLine.clear();
            continue;
        }

        if (ch == '\n')
        {
            if (m_activeLinePrefix.empty())
            {
                m_activeLinePrefix = makeTimestampPrefix();
            }

            pushCappedLine(m_activeLinePrefix + m_activeLine);
            m_activeLinePrefix.clear();
            m_activeLine.clear();
            continue;
        }

        if (m_activeLinePrefix.empty())
        {
            m_activeLinePrefix = makeTimestampPrefix();
        }
        m_activeLine.push_back(ch);
    }
}

void ConsolePanel::clear()
{
    std::lock_guard<std::mutex> lock(m_logsMutex);
    m_logs.clear();
    m_activeLinePrefix.clear();
    m_activeLine.clear();
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
        Settings::consolePanelIsMinimized = false;
        m_height = m_lastExpandedHeight;
    }
    else
    {
        m_lastExpandedHeight = m_height;
        m_minimized = true;
        Settings::consolePanelIsMinimized = true;
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

    std::deque<std::string> logsSnapshot;
    std::string activeLineSnapshot;
    {
        std::lock_guard<std::mutex> lock(m_logsMutex);
        logsSnapshot = m_logs;
        if (!m_activeLinePrefix.empty() || !m_activeLine.empty())
        {
            activeLineSnapshot = m_activeLinePrefix + m_activeLine;
        }
    }

    for (const auto& log : logsSnapshot)
    {
        ImGui::TextUnformatted(log.c_str());
    }

    if (!activeLineSnapshot.empty())
    {
        ImGui::TextUnformatted(activeLineSnapshot.c_str());
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::EndChild();
}