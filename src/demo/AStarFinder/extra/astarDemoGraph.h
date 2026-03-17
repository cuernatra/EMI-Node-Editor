#pragma once

#include <string>
#include <sstream>

namespace AStarDemo
{
inline std::string Generate()
{
    // General-use gameplay orchestration graph (Blueprint-style flow):
    //
    // Start -> Sequence
    //   Then0 -> astar_clearWalls()
    //   Then1 -> astar_setWall(4,4) -> astar_setWall(5,4) -> astar_runGame()
    //
    // Output is kept as a simple constant to satisfy graph return validation;
    // gameplay side-effects happen through flow-connected function calls.
    std::ostringstream s;

    // node <id> <Type> <pinCount> <pinIds...> <fieldCount> <field=val...> <x> <y>
    s << "node 1 Start 1 2 0 80.0 200.0\n";
    s << "node 3 Sequence 3 4 5 6 0 260.0 200.0\n";

    s << "node 7 FunctionCall 3 8 9 10 3 Name=astar_clearWalls ArgCount=0 ReturnType=None 450.0 120.0\n";

    s << "node 11 FunctionCall 5 12 13 14 15 16 3 Name=astar_setWall ArgCount=2 ReturnType=None 450.0 260.0\n";
    s << "node 17 Constant 1 18 2 Value=4.0 Type=Number 280.0 300.0\n";
    s << "node 19 Constant 1 20 2 Value=4.0 Type=Number 280.0 360.0\n";

    s << "node 21 FunctionCall 5 22 23 24 25 26 3 Name=astar_setWall ArgCount=2 ReturnType=None 700.0 260.0\n";
    s << "node 27 Constant 1 28 2 Value=5.0 Type=Number 540.0 300.0\n";
    s << "node 29 Constant 1 30 2 Value=4.0 Type=Number 540.0 360.0\n";

    s << "node 31 FunctionCall 3 32 33 34 3 Name=astar_runGame ArgCount=0 ReturnType=Boolean 940.0 260.0\n";

    s << "node 35 Constant 1 36 2 Value=true Type=Boolean 980.0 120.0\n";
    s << "node 37 Output 1 38 1 Label=game_result 1160.0 200.0\n";

    // link <id> <startPin> <endPin> <PinType>
    s << "link 39 2 4 Flow\n";   // Start.Exec -> Sequence.In
    s << "link 40 5 8 Flow\n";   // Sequence.Then0 -> clear.In
    s << "link 41 6 12 Flow\n";  // Sequence.Then1 -> setWallA.In
    s << "link 42 15 22 Flow\n"; // setWallA.Out -> setWallB.In
    s << "link 43 25 32 Flow\n"; // setWallB.Out -> runGame.In

    s << "link 44 18 13 Number\n"; // x=4 -> setWallA.Arg0
    s << "link 45 20 14 Number\n"; // y=4 -> setWallA.Arg1
    s << "link 46 28 23 Number\n"; // x=5 -> setWallB.Arg0
    s << "link 47 30 24 Number\n"; // y=4 -> setWallB.Arg1

    s << "link 48 36 38 Boolean\n"; // Output constant return

    return s.str();
}
}
