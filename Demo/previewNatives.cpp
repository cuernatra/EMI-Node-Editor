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

// Read the wall flag at (x, y). Returns 1.0 if wall, 0.0 if open.
static double astar_getwall_impl(double x, double y)
{
    if (!s_astar.initialized) return 0.0;
    const AStarRuntimeState& s = s_astar;
    const int ix = std::clamp(static_cast<int>(std::llround(x)), 0, s.w - 1);
    const int iy = std::clamp(static_cast<int>(std::llround(y)), 0, s.h - 1);
    return static_cast<double>(s.walls[static_cast<size_t>(iy * s.w + ix)]);
}

// Set or clear the wall at (x, y), then recompute the path.
// isWall != 0 places a wall; isWall == 0 removes it.
// Called by the graph's mouse interaction branch nodes.
static void astar_setwall_impl(double x, double y, double isWall)
{
    if (!s_astar.initialized) return;
    AStarRuntimeState& s = s_astar;
    const int ix = std::clamp(static_cast<int>(std::llround(x)), 0, s.w - 1);
    const int iy = std::clamp(static_cast<int>(std::llround(y)), 0, s.h - 1);

    // Never wall off the agent's current cell.
    if (ix == s.agentX && iy == s.agentY) return;

    const size_t i = static_cast<size_t>(iy * s.w + ix);
    const uint8_t newVal = (isWall != 0.0) ? 1 : 0;
    if (s.walls[i] == newVal) return; // no change, skip recompute

    s.walls[i] = newVal;
    s.path     = RunAStarPath(s.w, s.h, s.agentX, s.agentY, s.goalX, s.goalY, s.walls);
    s.pathStep = 1;
    if (s.path.size() < 2) Retarget();
}


// Number of path cells remaining (path.size() - pathStep).
// Used by the graph's path-drawing loop as its Count.
static double astar_getpathlen_impl()
{
    if (!s_astar.initialized) return 0.0;
    const AStarRuntimeState& s = s_astar;
    return (s.pathStep < s.path.size())
        ? static_cast<double>(s.path.size() - s.pathStep)
        : 0.0;
}

// X coordinate of the path cell at offset `index` from pathStep.
static double astar_getpathcell_x_impl(double index)
{
    if (!s_astar.initialized) return 0.0;
    const AStarRuntimeState& s = s_astar;
    const size_t i = s.pathStep + static_cast<size_t>(std::max(0LL, std::llround(index)));
    return (i < s.path.size()) ? static_cast<double>(s.path[i].x) : 0.0;
}

// Y coordinate of the path cell at offset `index` from pathStep.
static double astar_getpathcell_y_impl(double index)
{
    if (!s_astar.initialized) return 0.0;
    const AStarRuntimeState& s = s_astar;
    const size_t i = s.pathStep + static_cast<size_t>(std::max(0LL, std::llround(index)));
    return (i < s.path.size()) ? static_cast<double>(s.path[i].y) : 0.0;
}

// ---------------------------------------------------------------------------
// EMI registrations — A* only; primitives are registered in NodeGameFunctions.cpp
// ---------------------------------------------------------------------------

EMI_REGISTER(astar_init,           astar_init_impl)
EMI_REGISTER(astar_step,           astar_step_impl)
EMI_REGISTER(astar_get,            astar_get_impl)
EMI_REGISTER(astar_getwall,        astar_getwall_impl)
EMI_REGISTER(astar_setwall,        astar_setwall_impl)
EMI_REGISTER(astar_getpathlen,     astar_getpathlen_impl)
EMI_REGISTER(astar_getpathcell_x,  astar_getpathcell_x_impl)
EMI_REGISTER(astar_getpathcell_y,  astar_getpathcell_y_impl)
