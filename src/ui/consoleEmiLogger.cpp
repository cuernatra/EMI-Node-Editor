#include "consoleEmiLogger.h"

ConsoleEmiLogger::ConsoleEmiLogger(ConsolePanel& panel)
    : m_panel(panel)
{
}

void ConsoleEmiLogger::Print(const char* message)
{
    m_panel.addLogText(message ? message : "");
}

void ConsoleEmiLogger::PrintError(const char* message)
{
    if (message && message[0] != '\0')
    {
        m_panel.addLogText(std::string("ERROR: ") + message);
    }
    else
    {
        m_panel.addLogText("ERROR:");
    }
}
