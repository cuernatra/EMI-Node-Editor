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
    App app;
    app.run();

    return 0;
}