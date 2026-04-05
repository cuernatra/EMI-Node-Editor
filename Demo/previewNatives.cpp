// A* pathfinder helpers for the node-editor demo.
// Primitive drawing/input functions live in NodeGameFunctions.cpp.
#include "previewNatives.h"
#include "NodeGameFunctions.h"
#include "editor/panels/renderPanel.h"
#include <EMI/EMI.h>
#include <Defines.h>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <queue>
#include <random>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// A* runtime state (thread-local — each VM thread runs its own simulation)
// ---------------------------------------------------------------------------

struct PathCell { int x = 0; int y = 0; };

struct AStarRuntimeState
{
    bool initialized    = false;
    bool wallsInitialized = false;
    int  w = 20, h = 20;
    int  agentX = 0, agentY = 0;
    int  goalX  = 0, goalY  = 0;
    std::vector<uint8_t>   walls;
    std::vector<PathCell>  path;
    size_t                 pathStep = 1;
    std::mt19937           rng{ std::random_device{}() };
};

static thread_local AStarRuntimeState s_astar;

// ---------------------------------------------------------------------------
// Core A* algorithm
// ---------------------------------------------------------------------------

static std::vector<PathCell> RunAStarPath(
    int w, int h, int sx, int sy, int gx, int gy,
    const std::vector<uint8_t>& walls)
{
    auto idx       = [w](int x, int y)      { return y * w + x; };
    auto inBounds  = [w, h](int x, int y)   { return x >= 0 && y >= 0 && x < w && y < h; };
    auto heuristic = [gx, gy](int x, int y) { return std::abs(gx - x) + std::abs(gy - y); };

    const int start = idx(sx, sy);
    const int goal  = idx(gx, gy);
    if (start < 0 || goal < 0 ||
        start >= static_cast<int>(walls.size()) ||
        goal  >= static_cast<int>(walls.size()))
        return {};
    if (walls[static_cast<size_t>(start)] || walls[static_cast<size_t>(goal)])
        return {};

    std::vector<int> came(static_cast<size_t>(w * h), -1);
    std::vector<int> g(static_cast<size_t>(w * h), std::numeric_limits<int>::max());

    struct OpenItem {
        int f, i;
        bool operator<(const OpenItem& rhs) const { return f > rhs.f; }
    };

    std::priority_queue<OpenItem> open;
    g[static_cast<size_t>(start)] = 0;
    open.push({ heuristic(sx, sy), start });

    static constexpr int kDirs[4][2] = { { -1,0 }, { 1,0 }, { 0,-1 }, { 0,1 } };

    while (!open.empty())
    {
        const int cur = open.top().i;
        open.pop();
        if (cur == goal) break;

        const int cx = cur % w, cy = cur / w;
        for (const auto& d : kDirs)
        {
            const int nx = cx + d[0], ny = cy + d[1];
            if (!inBounds(nx, ny)) continue;
            const int ni = idx(nx, ny);
            if (walls[static_cast<size_t>(ni)]) continue;

            const int cand = g[static_cast<size_t>(cur)] + 1;
            if (cand < g[static_cast<size_t>(ni)])
            {
                g[static_cast<size_t>(ni)]    = cand;
                came[static_cast<size_t>(ni)] = cur;
                open.push({ cand + heuristic(nx, ny), ni });
            }
        }
    }

    if (came[static_cast<size_t>(goal)] == -1 && start != goal)
        return {};

    std::vector<PathCell> path;
    for (int at = goal;;)
    {
        path.push_back({ at % w, at / w });
        if (at == start) break;
        at = came[static_cast<size_t>(at)];
        if (at < 0) return {};
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// ---------------------------------------------------------------------------
// Rendering helpers
// ---------------------------------------------------------------------------

static void DrawFrame()
{
    RenderPanel* panel = GetNativeRenderPanel();
    if (!panel) return;

    const AStarRuntimeState& s = s_astar;
    panel->clearGrid(s.w, s.h);

    // Walls — grey
    for (int y = 0; y < s.h; ++y)
        for (int x = 0; x < s.w; ++x)
            if (s.walls[static_cast<size_t>(y * s.w + x)])
                panel->drawCell(x, y, 75, 75, 75);

    // Path — blue
    for (size_t i = s.pathStep; i < s.path.size(); ++i)
    {
        const PathCell& c = s.path[i];
        if ((c.x == s.goalX && c.y == s.goalY) || (c.x == s.agentX && c.y == s.agentY))
            continue;
        panel->drawCell(c.x, c.y, 0, 100, 220);
    }

    // Goal — green, agent — red
    panel->drawCell(s.goalX,  s.goalY,  0,   180, 60);
    panel->drawCell(s.agentX, s.agentY, 220, 50,  50);
    panel->renderGrid(); // no-op but keeps symmetry with the original
}

// ---------------------------------------------------------------------------
// Wall generation
// ---------------------------------------------------------------------------

static void GenerateWalls()
{
    AStarRuntimeState& s = s_astar;
    if (s.wallsInitialized) return;

    s.walls.assign(static_cast<size_t>(s.w * s.h), 0);
    std::bernoulli_distribution wallDist(0.20);
    auto idx = [&s](int x, int y) { return y * s.w + x; };

    auto tryFill = [&]()
    {
        for (int y = 0; y < s.h; ++y)
            for (int x = 0; x < s.w; ++x)
                s.walls[static_cast<size_t>(idx(x, y))] = wallDist(s.rng) ? 1 : 0;
        s.walls[static_cast<size_t>(idx(s.agentX, s.agentY))] = 0;
        s.walls[static_cast<size_t>(idx(s.goalX,  s.goalY))]  = 0;
    };

    for (int attempt = 0; attempt < 16; ++attempt)
    {
        tryFill();
        if (RunAStarPath(s.w, s.h, s.agentX, s.agentY, s.goalX, s.goalY, s.walls).size() >= 2)
            break;
    }

    // Fallback: open grid
    if (RunAStarPath(s.w, s.h, s.agentX, s.agentY, s.goalX, s.goalY, s.walls).size() < 2)
        std::fill(s.walls.begin(), s.walls.end(), 0);

    s.wallsInitialized = true;
}

// ---------------------------------------------------------------------------
// Retarget: pick a new reachable goal
// ---------------------------------------------------------------------------

static void Retarget()
{
    AStarRuntimeState& s = s_astar;
    std::uniform_int_distribution<int> dx(0, s.w - 1);
    std::uniform_int_distribution<int> dy(0, s.h - 1);

    for (int attempt = 0; attempt < 100; ++attempt)
    {
        const int nx = dx(s.rng), ny = dy(s.rng);
        if (nx == s.agentX && ny == s.agentY) continue;
        if (s.walls[static_cast<size_t>(ny * s.w + nx)]) continue;
        s.goalX = nx;
        s.goalY = ny;
        s.path  = RunAStarPath(s.w, s.h, s.agentX, s.agentY, s.goalX, s.goalY, s.walls);
        s.pathStep = 1;
        if (s.path.size() >= 2) return;
    }
}

// ---------------------------------------------------------------------------
// Native implementations
// ---------------------------------------------------------------------------

static void astar_init_impl(double gridW, double gridH,
                            double startX, double startY,
                            double goalX,  double goalY)
{
    if (!GetNativeRenderPanel()) return;

    AStarRuntimeState& s = s_astar;
    s.w      = std::max(2, static_cast<int>(std::llround(gridW)));
    s.h      = std::max(2, static_cast<int>(std::llround(gridH)));
    s.agentX = std::clamp(static_cast<int>(std::llround(startX)), 0, s.w - 1);
    s.agentY = std::clamp(static_cast<int>(std::llround(startY)), 0, s.h - 1);
    s.goalX  = std::clamp(static_cast<int>(std::llround(goalX)),  0, s.w - 1);
    s.goalY  = std::clamp(static_cast<int>(std::llround(goalY)),  0, s.h - 1);

    if (!s.wallsInitialized || s.walls.size() != static_cast<size_t>(s.w * s.h))
    {
        s.wallsInitialized = false;
        GenerateWalls();
    }

    s.walls[static_cast<size_t>(s.agentY * s.w + s.agentX)] = 0;
    s.walls[static_cast<size_t>(s.goalY  * s.w + s.goalX)]  = 0;
    s.initialized = true;

    s.path     = RunAStarPath(s.w, s.h, s.agentX, s.agentY, s.goalX, s.goalY, s.walls);
    s.pathStep = 1;
    if (s.path.size() < 2) Retarget();
}

static void astar_step_impl()
{
    if (!GetNativeRenderPanel() || !s_astar.initialized) return;

    AStarRuntimeState& s = s_astar;
    if (s.pathStep < s.path.size())
    {
        s.agentX = s.path[s.pathStep].x;
        s.agentY = s.path[s.pathStep].y;
        ++s.pathStep;
        return;
    }

    // Reached goal — pick new target
    Retarget();
}

static void astar_render_impl()
{
    if (!GetNativeRenderPanel() || !s_astar.initialized) return;
    DrawFrame();
}

static void astar_retarget_impl()
{
    if (!GetNativeRenderPanel() || !s_astar.initialized) return;
    Retarget();
}

static double astar_get_impl(double keyId)
{
    if (!s_astar.initialized) return 0.0;
    const AStarRuntimeState& s = s_astar;
    switch (static_cast<int>(std::llround(keyId)))
    {
        case 0: return static_cast<double>(s.agentX);
        case 1: return static_cast<double>(s.agentY);
        case 2: return static_cast<double>(s.goalX);
        case 3: return static_cast<double>(s.goalY);
        case 4: return s.pathStep < s.path.size()
                       ? static_cast<double>(s.path.size() - s.pathStep) : 0.0;
        case 5: { double n = 0; for (uint8_t w : s.walls) n += (w ? 1.0 : 0.0); return n; }
        default: return 0.0;
    }
}

static bool astar_needs_retarget_impl()
{
    if (!s_astar.initialized) return true;
    return s_astar.pathStep >= s_astar.path.size();
}

// Poll mouse state and paint/erase walls in the live A* grid.
// Left button  → place wall at cursor cell and recompute path.
// Right button → erase wall at cursor cell and recompute path.
static void astar_interact_impl()
{
    RenderPanel* panel = GetNativeRenderPanel();
    if (!panel || !s_astar.initialized) return;

    AStarRuntimeState& s = s_astar;
    const int x = panel->getMouseX();
    const int y = panel->getMouseY();
    if (x < 0 || y < 0 || x >= s.w || y >= s.h) return;

    const size_t i = static_cast<size_t>(y * s.w + x);

    if (panel->isMouseButtonDown(0))
    {
        // Don't wall-off the agent's current cell.
        if (x == s.agentX && y == s.agentY) return;
        if (s.walls[i] == 1) return; // already a wall, skip recompute
        s.walls[i] = 1;
        s.path     = RunAStarPath(s.w, s.h, s.agentX, s.agentY, s.goalX, s.goalY, s.walls);
        s.pathStep = 1;
        if (s.path.size() < 2) Retarget();
    }
    else if (panel->isMouseButtonDown(1))
    {
        if (s.walls[i] == 0) return; // already empty, skip recompute
        s.walls[i] = 0;
        s.path     = RunAStarPath(s.w, s.h, s.agentX, s.agentY, s.goalX, s.goalY, s.walls);
        s.pathStep = 1;
    }
}

// Blocking all-in-one loop — lets a single NativeCall node run the whole demo.
static void astarpathfinder_impl(double gridW, double gridH,
                                 double startX, double startY,
                                 double goalX,  double goalY)
{
    astar_init_impl(gridW, gridH, startX, startY, goalX, goalY);
    while (!IsRuntimeInterruptRequested())
    {
        astar_step_impl();
        astar_render_impl();
        for (int i = 0; i < 5 && !IsRuntimeInterruptRequested(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// ---------------------------------------------------------------------------
// EMI registrations — A* only; primitives are registered in NodeGameFunctions.cpp
// ---------------------------------------------------------------------------

EMI_REGISTER(astar_init,           astar_init_impl)
EMI_REGISTER(astar_step,           astar_step_impl)
EMI_REGISTER(astar_render,         astar_render_impl)
EMI_REGISTER(astar_retarget,       astar_retarget_impl)
EMI_REGISTER(astar_get,            astar_get_impl)
EMI_REGISTER(astar_needs_retarget, astar_needs_retarget_impl)
EMI_REGISTER(astar_interact,       astar_interact_impl)
EMI_REGISTER(astarpathfinder,      astarpathfinder_impl)
