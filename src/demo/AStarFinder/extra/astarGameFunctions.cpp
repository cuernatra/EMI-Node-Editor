/**
 * @file astarGameFunctions.cpp
 * @brief A* pathfinding C++ implementation registered with EMI.
 *
 * Ports the core pathfinding logic from the game.ril AStarFinder demo into
 * C++ and exposes it to the node-editor graph compiler via EMI_REGISTER.
 *
 * The algorithm is a straightforward A* on a 10×10 grid with 4-directional
 * movement and a Euclidean distance heuristic (matching game.ril's GetCost).
 */

#include "EMI/EMI.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
#include <cstring>
#include <random>
#include <climits>

// ---------------------------------------------------------------------------
// Grid state  (matches game.ril constants: x_size=40, y_size=40 but we
// use a compact 10×10 grid for the computational demo)
// ---------------------------------------------------------------------------

static constexpr int GRID_W = 10;
static constexpr int GRID_H = 10;

static bool sWalls[GRID_H][GRID_W] = {};   // true = wall / impassable

// Four cardinal directions matching game.ril Actions array:
//   Coord{-1,0}, Coord{1,0}, Coord{0,-1}, Coord{0,1}
static const int kDX[4] = { -1,  1,  0, 0 };
static const int kDY[4] = {  0,  0, -1, 1 };

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool inBounds(int x, int y)
{
    return x >= 0 && x < GRID_W && y >= 0 && y < GRID_H;
}

static bool passable(int x, int y)
{
    return inBounds(x, y) && !sWalls[y][x];
}

struct PathNode
{
    int x = 0;
    int y = 0;
    int parent = -1;
    int g = 0;
    int f = 0;
};

struct GridCoord
{
    int x = 0;
    int y = 0;
};

static bool cellBlocked(int x, int y)
{
    if (!inBounds(x, y))
        return true;
    return sWalls[y][x];
}

static int heuristicCost(int x, int y, int tx, int ty)
{
    const int dx = tx - x;
    const int dy = ty - y;
    return static_cast<int>(2.0 * std::sqrt(static_cast<double>(dx * dx + dy * dy)));
}

static std::vector<int> findPathActions(int startX, int startY, int targetX, int targetY)
{
    std::vector<PathNode> open;
    std::vector<PathNode> nodes;
    bool closed[GRID_H][GRID_W] = {};

    auto pushNode = [&](int x, int y, int parent, int g)
    {
        PathNode node;
        node.x = x;
        node.y = y;
        node.parent = parent;
        node.g = g;
        node.f = g + heuristicCost(x, y, targetX, targetY);
        nodes.push_back(node);
        open.push_back(node);
    };

    pushNode(startX, startY, -1, 0);

    int foundIndex = -1;

    while (!open.empty())
    {
        auto it = std::min_element(open.begin(), open.end(),
            [](const PathNode& a, const PathNode& b) { return a.f < b.f; });

        PathNode current = *it;
        open.erase(it);

        if (closed[current.y][current.x])
            continue;
        closed[current.y][current.x] = true;

        int currentIndex = -1;
        for (int i = static_cast<int>(nodes.size()) - 1; i >= 0; --i)
        {
            if (nodes[static_cast<size_t>(i)].x == current.x &&
                nodes[static_cast<size_t>(i)].y == current.y &&
                nodes[static_cast<size_t>(i)].g == current.g)
            {
                currentIndex = i;
                break;
            }
        }

        if (current.x == targetX && current.y == targetY)
        {
            foundIndex = currentIndex;
            break;
        }

        for (int action = 0; action < 4; ++action)
        {
            const int nx = current.x + kDX[action];
            const int ny = current.y + kDY[action];

            if (!inBounds(nx, ny) || closed[ny][nx] || cellBlocked(nx, ny))
                continue;

            int bestKnown = INT_MAX;
            for (const PathNode& node : nodes)
            {
                if (node.x == nx && node.y == ny)
                    bestKnown = std::min(bestKnown, node.g);
            }

            const int ng = current.g + 1;
            if (ng < bestKnown)
                pushNode(nx, ny, currentIndex, ng);
        }
    }

    std::vector<int> actions;
    if (foundIndex < 0)
        return actions;

    int idx = foundIndex;
    while (idx >= 0)
    {
        const PathNode& cur = nodes[static_cast<size_t>(idx)];
        if (cur.parent < 0)
            break;

        const PathNode& prev = nodes[static_cast<size_t>(cur.parent)];
        const int dx = cur.x - prev.x;
        const int dy = cur.y - prev.y;

        for (int action = 0; action < 4; ++action)
        {
            if (kDX[action] == dx && kDY[action] == dy)
            {
                actions.push_back(action);
                break;
            }
        }

        idx = cur.parent;
    }

    std::reverse(actions.begin(), actions.end());
    return actions;
}

// ---------------------------------------------------------------------------
// EMI-callable API implementations
// ---------------------------------------------------------------------------

/** Reset the grid – no walls anywhere. */
static void impl_clearWalls()
{
    std::memset(sWalls, 0, sizeof(sWalls));
}

/** Mark cell (x, y) as a wall. Out-of-bounds calls are silently ignored. */
static void impl_setWall(double x, double y)
{
    const int ix = static_cast<int>(x);
    const int iy = static_cast<int>(y);
    if (inBounds(ix, iy))
        sWalls[iy][ix] = true;
}

/**
 * Run A* from (sx,sy) to (ex,ey) on the current grid.
 * Returns the path length (number of steps) or -1 if no path exists.
 *
 * Matches the heuristic in game.ril's GetCost:
 *   f = g + 2 * sqrt(dx*dx + dy*dy)
 */
static double impl_findPath(double sx, double sy, double ex, double ey)
{
    const int startX = static_cast<int>(sx);
    const int startY = static_cast<int>(sy);
    const int endX   = static_cast<int>(ex);
    const int endY   = static_cast<int>(ey);

    if (!passable(startX, startY) || !passable(endX, endY))
        return -1.0;

    if (startX == endX && startY == endY)
        return 0.0;

    // A* open list entry.
    struct OpenEntry
    {
        int    x, y;
        double g, f;
    };

    // Per-cell tracking arrays.
    double gCost[GRID_H][GRID_W];
    bool   closed[GRID_H][GRID_W];
    for (int r = 0; r < GRID_H; ++r)
        for (int c = 0; c < GRID_W; ++c)
        {
            gCost [r][c] = 1e9;
            closed[r][c] = false;
        }

    auto heuristic = [&](int x, int y) -> double
    {
        const double dx = static_cast<double>(endX - x);
        const double dy = static_cast<double>(endY - y);
        return 2.0 * std::sqrt(dx * dx + dy * dy);
    };

    std::vector<OpenEntry> open;
    open.reserve(GRID_W * GRID_H);

    gCost[startY][startX] = 0.0;
    open.push_back({ startX, startY, 0.0, heuristic(startX, startY) });

    while (!open.empty())
    {
        // Pop the node with lowest f.
        const auto minIt = std::min_element(open.begin(), open.end(),
            [](const OpenEntry& a, const OpenEntry& b) { return a.f < b.f; });

        const OpenEntry cur = *minIt;
        open.erase(minIt);

        if (cur.x == endX && cur.y == endY)
            return cur.g;   // path length in steps

        if (closed[cur.y][cur.x])
            continue;
        closed[cur.y][cur.x] = true;

        for (int d = 0; d < 4; ++d)
        {
            const int nx = cur.x + kDX[d];
            const int ny = cur.y + kDY[d];

            if (!passable(nx, ny) || closed[ny][nx])
                continue;

            const double ng = cur.g + 1.0;   // GetStateCost always returns 1.0
            if (ng < gCost[ny][nx])
            {
                gCost[ny][nx] = ng;
                open.push_back({ nx, ny, ng, ng + heuristic(nx, ny) });
            }
        }
    }

    return -1.0;   // no path
}

static bool impl_runGame()
{
    constexpr int kCellSize = 42;
    constexpr int kMargin = 24;

    const int width = GRID_W * kCellSize + kMargin * 2;
    const int height = GRID_H * kCellSize + kMargin * 2;

    sf::RenderWindow window(
        sf::VideoMode(static_cast<unsigned int>(width), static_cast<unsigned int>(height)),
        "A* Finder Demo",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);

    GridCoord player{0, 0};
    GridCoord target{GRID_W - 1, GRID_H - 1};

    std::vector<int> actions;
    int actionIdx = 0;

    std::mt19937 rng(static_cast<unsigned int>(std::random_device{}()));
    std::uniform_int_distribution<int> distX(0, GRID_W - 1);
    std::uniform_int_distribution<int> distY(0, GRID_H - 1);

    sf::Clock moveClock;
    constexpr float kMoveInterval = 0.09f;

    bool running = true;
    while (running && window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                running = false;
                window.close();
                break;
            }

            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
            {
                running = false;
                window.close();
                break;
            }

            if (event.type == sf::Event::MouseButtonPressed)
            {
                const int gx = (event.mouseButton.x - kMargin) / kCellSize;
                const int gy = (event.mouseButton.y - kMargin) / kCellSize;

                if (inBounds(gx, gy) && !(gx == player.x && gy == player.y))
                {
                    if (event.mouseButton.button == sf::Mouse::Left)
                        sWalls[gy][gx] = true;
                    else if (event.mouseButton.button == sf::Mouse::Right)
                        sWalls[gy][gx] = false;

                    actions.clear();
                    actionIdx = 0;
                }
            }
        }

        if (running)
        {
            if (actionIdx >= static_cast<int>(actions.size()))
            {
                // Pick a new reachable random target on empty cell.
                do
                {
                    target.x = distX(rng);
                    target.y = distY(rng);
                }
                while (cellBlocked(target.x, target.y) || (target.x == player.x && target.y == player.y));

                actions = findPathActions(player.x, player.y, target.x, target.y);
                actionIdx = 0;
            }

            if (actionIdx < static_cast<int>(actions.size()) && moveClock.getElapsedTime().asSeconds() >= kMoveInterval)
            {
                moveClock.restart();

                const int action = actions[static_cast<size_t>(actionIdx++)];
                const int nx = player.x + kDX[action];
                const int ny = player.y + kDY[action];

                if (inBounds(nx, ny) && !cellBlocked(nx, ny))
                {
                    player.x = nx;
                    player.y = ny;
                }
                else
                {
                    actions.clear();
                    actionIdx = 0;
                }
            }
        }

        window.clear(sf::Color(26, 28, 35));

        sf::RectangleShape cellShape(sf::Vector2f(static_cast<float>(kCellSize - 1), static_cast<float>(kCellSize - 1)));

        for (int y = 0; y < GRID_H; ++y)
        {
            for (int x = 0; x < GRID_W; ++x)
            {
                cellShape.setPosition(
                    static_cast<float>(kMargin + x * kCellSize),
                    static_cast<float>(kMargin + y * kCellSize)
                );

                if (sWalls[y][x])
                    cellShape.setFillColor(sf::Color(30, 30, 30));
                else
                    cellShape.setFillColor(sf::Color(215, 220, 230));

                window.draw(cellShape);
            }
        }

        // Draw current planned path preview.
        {
            GridCoord probe = player;
            sf::RectangleShape pathCell(sf::Vector2f(static_cast<float>(kCellSize - 8), static_cast<float>(kCellSize - 8)));
            pathCell.setFillColor(sf::Color(80, 145, 255));

            for (size_t i = static_cast<size_t>(actionIdx); i < actions.size(); ++i)
            {
                probe.x += kDX[actions[i]];
                probe.y += kDY[actions[i]];
                if (!inBounds(probe.x, probe.y) || sWalls[probe.y][probe.x])
                    break;

                pathCell.setPosition(
                    static_cast<float>(kMargin + probe.x * kCellSize + 4),
                    static_cast<float>(kMargin + probe.y * kCellSize + 4)
                );
                window.draw(pathCell);
            }
        }

        // Draw target
        {
            sf::CircleShape t(static_cast<float>(kCellSize / 2 - 6));
            t.setFillColor(sf::Color(75, 220, 120));
            t.setPosition(
                static_cast<float>(kMargin + target.x * kCellSize + 6),
                static_cast<float>(kMargin + target.y * kCellSize + 6)
            );
            window.draw(t);
        }

        // Draw player
        {
            sf::CircleShape p(static_cast<float>(kCellSize / 2 - 8));
            p.setFillColor(sf::Color(235, 80, 80));
            p.setPosition(
                static_cast<float>(kMargin + player.x * kCellSize + 8),
                static_cast<float>(kMargin + player.y * kCellSize + 8)
            );
            window.draw(p);
        }

        window.display();
    }

    return true;
}

// ---------------------------------------------------------------------------
// EMI registration (happens at static-init time)
// ---------------------------------------------------------------------------

EMI_REGISTER(astar_clearWalls, impl_clearWalls)
EMI_REGISTER(astar_setWall,    impl_setWall)
EMI_REGISTER(astar_findPath,   impl_findPath)
EMI_REGISTER(astar_runGame,    impl_runGame)
