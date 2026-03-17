/**
 * @file astarGameFunctions.h
 * @brief A* pathfinding functions registered with EMI for the node-editor demo.
 *
 * Implements a simple grid-based A* algorithm and registers the following
 * functions so they can be called from the visual node graph:
 *
 *   astar_clearWalls()              – resets the 10×10 grid (no walls)
 *   astar_setWall(x, y)             – marks cell (x,y) as impassable
 *   astar_findPath(sx,sy,ex,ey)     – runs A* on the grid, returns path length
 *                                     or -1 when no path exists
 *
 * Registration is performed via EMI_REGISTER at static-init time; simply
 * including this translation unit (via CMakeLists) is sufficient.
 *
 * The GameFunctions.cpp file from the original AStarFinder demo is also
 * included as-is and its console helper symbols are available in the same
 * link unit, but they are not registered as EMI functions here because the
 * editor runs inside an SFML window, not a Windows console application.
 */

#pragma once

// No public API needed beyond the EMI_REGISTER side-effects.
