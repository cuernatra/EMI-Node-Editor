#include <iostream>
#include "app/app.h"
#include "EMI/EMI.h"

int main()
{
    EMI::SetLogLevel(EMI::LogLevel::Info);
    auto env = EMI::CreateEnvironment();

    std::cout << "ENV OK\n";

    auto script = env.CompileTemporary("1 + 2;");
    std::cout << "TEST CODE COMPILED\n";

    App app;
    app.run();

    std::cout << "APP EXIT\n";
    EMI::ReleaseEnvironment(env);
    return 0;
}