#pragma once

#include "consolePanel.h"

#if __has_include(<EMI/EMI.h>)
#include <EMI/EMI.h>
#elif __has_include("EMI/EMI.h")
#include "EMI/EMI.h"
#elif __has_include("../../EMI-Script/EMI-Script-master/include/EMI/EMI.h")
#include "../../EMI-Script/EMI-Script-master/include/EMI/EMI.h"
#else
#error "Unable to locate EMI/EMI.h"
#endif

class ConsoleEmiLogger final : public EMI::Logger
{
public:
    explicit ConsoleEmiLogger(ConsolePanel& panel);

    void Print(const char* message) override;
    void PrintError(const char* message) override;

private:
    ConsolePanel& m_panel;
};
