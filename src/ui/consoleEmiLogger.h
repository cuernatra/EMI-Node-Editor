#pragma once

#include "consolePanel.h"
#include <EMI/EMI.h>

class ConsoleEmiLogger final : public EMI::Logger
{
public:
    explicit ConsoleEmiLogger(ConsolePanel& panel);

    void Print(const char* message) override;
    void PrintError(const char* message) override;

private:
    ConsolePanel& m_panel;
};
