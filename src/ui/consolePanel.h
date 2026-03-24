/**
 * @brief Console panel for editor output and logs
 * Display area for compiler messages, runtime logs, and other output.
 */
#ifndef CONSOLEPANEL_H
#define CONSOLEPANEL_H

#include "../app/constants.h"
#include <cstddef>
#include <deque>
#include <string>
#include <mutex>

/**
 * @brief Console panel class
 * Manages the display and functionality of the console output area.
 */
class ConsolePanel
{
public:
    /// Initialize the console panel
    ConsolePanel();
    
    /// Render the console panel (called every frame).
    void draw();
    
    /// Add a log message to the console
    void addLog(const char* fmt, ...) IM_FMTARGS(2);

    /// Add a preformatted message to the console (thread-safe)
    void addLogText(const std::string& message);

    /// Clear all log messages
    void clear();

    /// Get the height of the console panel
    float getHeight() const;

    /// Set the height of the console panel
    void setHeight(float height);

    /// Toggle between minimized and expanded states
    void toggleMinimized();

    /// Returns true if the console is currently minimized
    bool isMinimized() const;
    
private:
    static constexpr std::size_t kMaxLogLines = 10000;

    /// Height of the console panel in pixels
    float m_height;
    /// Last expanded height used when restoring after minimize
    float m_lastExpandedHeight;
    /// Tracks whether the console is minimized
    bool m_minimized;
    std::mutex m_logsMutex;
    std::deque<std::string> m_logs;
    std::string m_activeLinePrefix;
    std::string m_activeLine;

    std::string makeTimestampPrefix() const;
    std::string withTimestamp(const std::string& message) const;
    void pushCappedLine(const std::string& line);
};

#endif