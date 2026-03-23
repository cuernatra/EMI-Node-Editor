/**
 * @brief Console panel for editor output and logs
 * Display area for compiler messages, runtime logs, and other output.
 */
#ifndef CONSOLEPANEL_H
#define CONSOLEPANEL_H

#include "../app/constants.h"
#include <vector>
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

    /// Clear all log messages
    void clear();

    /// Get the height of the console panel
    float getHeight() const;
    
private:
    /// Height of the console panel in pixels
    float m_height;
    std::vector<std::string> m_logs;
};

#endif