#include <iostream>
#include "app/app.h"
#include "EMI/EMI.h"
#include "Parser/Node.h"

static void print_num(double x)
{
    std::cout << x << "\n";
}

EMI_REGISTER(print, print_num)

int main()
{
    std::cout << "START\n";

    EMI::SetLogLevel(EMI::LogLevel::Debug);

    Node test;
    test.data = 123.0;
    std::cout << VariantToInt(&test) << "\n";

    auto env = EMI::CreateEnvironment();
    std::cout << "ENV OK\n";

    auto script = env.CompileTemporary("print(1 + 2);");
    if (!script.wait()) {
        std::cout << "SCRIPT FAIL\n";
    } else {
        std::cout << "SCRIPT DONE\n";
    }

    App app;
    app.run();

    std::cout << "APP EXIT\n";
    EMI::ReleaseEnvironment(env);
    return 0;
}