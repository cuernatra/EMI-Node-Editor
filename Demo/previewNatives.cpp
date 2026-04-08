
// ===============================
// A* pathfinder bridge natives for the node-editor demo.
// Each EMI_REGISTER'd function is intended to map 1-to-1 with a node in the visual graph.
// Primitive drawing/input functions live in NodeGameFunctions.cpp.
//
// Node implementation notes:
// - Each function below is a candidate for a node.
// - Inputs/outputs are explicit and should be preserved for node wiring.
// - Internal state is kept in s_astar, which would become node state or graph variables.
// ===============================

#include "previewNatives.h"
#include "NodeGameFunctions.h"
#include "editor/panels/renderPanel.h"
#include <EMI/EMI.h>
#include <Defines.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <limits>
#include <queue>
#include <random>
#include <thread>
#include <vector>

// -----------------------------------------------------------------------------
// Graph stop helpers (used by Ticker/Delay nodes)
// -----------------------------------------------------------------------------

static std::atomic<std::atomic<bool>*> g_graphForceStopFlag{ nullptr };

void SetGraphForceStopFlag(std::atomic<bool>* flag)
{
    g_graphForceStopFlag.store(flag, std::memory_order_release);
}

static bool __emi_force_stop_requested_impl()
{
    std::atomic<bool>* flag = g_graphForceStopFlag.load(std::memory_order_acquire);
    return flag && flag->load(std::memory_order_relaxed);
}

static void __emi_delay_impl(double delayMs)
{
    if (delayMs <= 0.0)
        return;

    using namespace std::chrono;
    auto remaining = milliseconds(static_cast<int64_t>(delayMs));
    if (remaining.count() <= 0)
        return;

    // Sleep in small chunks so Force Stop can interrupt long waits.
    while (remaining.count() > 0 && !__emi_force_stop_requested_impl())
    {
        const auto chunk = std::min<milliseconds>(remaining, milliseconds(10));
        std::this_thread::sleep_for(chunk);
        remaining -= chunk;
    }
}


// -----------------------------------------------------------------------------
// Shared runtime state — one instance per VM thread (would become graph state)
// -----------------------------------------------------------------------------

struct PathCell { int x = 0, y = 0; };

struct AStarState
{
    bool initialized      = false;
    bool wallsInitialized = false;
    int  w = 20, h = 20;
    int  agentX = 0, agentY = 0;
    int  goalX  = 0, goalY  = 0;
    std::vector<uint8_t>  walls;    // flat layout: walls[y*w + x]
    std::vector<PathCell> path;
    size_t                pathStep = 1;
    std::mt19937          rng{ std::random_device{}() };
};

static thread_local AStarState s_astar;


// -----------------------------------------------------------------------------
// Shared helpers — not nodes (utility functions)
// -----------------------------------------------------------------------------

static int  ToInt(double v)         { return static_cast<int>(std::llround(v)); }
static int  CellIdx(int x, int y)   { return y * s_astar.w + x; }
static bool InBounds(int x, int y)  { return x >= 0 && y >= 0 && x < s_astar.w && y < s_astar.h; }



// -----------------------------------------------------------------------------
// NODE CANDIDATES — dependency order: findpath → initwalls → retarget → init/step/setwall
// -----------------------------------------------------------------------------


// Node: astar_findpath
// Inputs: startX (number), startY (number), goalX (number), goalY (number)
// Output: number (path length, 0 = no path)
// Description: Runs A* and stores the result in s_astar.path. Returns path length.
static double astar_findpath_impl(double startX, double startY,
                                  double goalX,  double goalY)
{
    if (!s_astar.initialized && s_astar.walls.empty()) return 0.0;

    const int sx = std::clamp(ToInt(startX), 0, s_astar.w - 1);
    const int sy = std::clamp(ToInt(startY), 0, s_astar.h - 1);
    const int gx = std::clamp(ToInt(goalX),  0, s_astar.w - 1);
    const int gy = std::clamp(ToInt(goalY),  0, s_astar.h - 1);

    if (!InBounds(sx, sy) || !InBounds(gx, gy)) { s_astar.path.clear(); return 0.0; }

    const int start = CellIdx(sx, sy);
    const int goal  = CellIdx(gx, gy);
    if (s_astar.walls[start] || s_astar.walls[goal]) { s_astar.path.clear(); return 0.0; }

    auto heuristic = [gx, gy](int x, int y) { return std::abs(gx - x) + std::abs(gy - y); };

    const int total = s_astar.w * s_astar.h;
    std::vector<int> came(total, -1);
    std::vector<int> g(total, std::numeric_limits<int>::max());

    struct OpenItem { int f, i; bool operator<(const OpenItem& o) const { return f > o.f; } };
    std::priority_queue<OpenItem> open;
    g[start] = 0;
    open.push({ heuristic(sx, sy), start });

    static constexpr int kDirs[4][2] = { {-1,0},{1,0},{0,-1},{0,1} };
    while (!open.empty())
    {
        int cur = open.top().i; open.pop();
        if (cur == goal) break;
        int cx = cur % s_astar.w, cy = cur / s_astar.w;
        for (const auto& d : kDirs)
        {
            int nx = cx + d[0], ny = cy + d[1];
            if (!InBounds(nx, ny)) continue;
            int ni = CellIdx(nx, ny);
            if (s_astar.walls[ni]) continue;
            int cand = g[cur] + 1;
            if (cand < g[ni]) { g[ni] = cand; came[ni] = cur; open.push({ cand + heuristic(nx, ny), ni }); }
        }
    }

    if (came[goal] == -1 && start != goal) { s_astar.path.clear(); return 0.0; }

    s_astar.path.clear();
    for (int at = goal;;)
    {
        s_astar.path.push_back({ at % s_astar.w, at / s_astar.w });
        if (at == start) break;
        at = came[at];
        if (at < 0) { s_astar.path.clear(); return 0.0; }
    }
    std::reverse(s_astar.path.begin(), s_astar.path.end());
    s_astar.pathStep = 1;

    return (double)s_astar.path.size();
}


// Node: astar_initwalls
// Inputs: (none)
// Output: void
// Description: Randomly fills s_astar.walls, retrying until a valid path exists.
static void astar_initwalls_impl()
{
    if (s_astar.wallsInitialized) return;

    s_astar.walls.assign(s_astar.w * s_astar.h, 0);
    std::bernoulli_distribution wallDist(0.20);

    auto tryFill = [&]()
    {
        for (int y = 0; y < s_astar.h; ++y)
            for (int x = 0; x < s_astar.w; ++x)
                s_astar.walls[CellIdx(x, y)] = wallDist(s_astar.rng) ? 1 : 0;
        s_astar.walls[CellIdx(s_astar.agentX, s_astar.agentY)] = 0;
        s_astar.walls[CellIdx(s_astar.goalX,  s_astar.goalY)]  = 0;
    };

    for (int attempt = 0; attempt < 16; ++attempt)
    {
        tryFill();
        if (astar_findpath_impl(s_astar.agentX, s_astar.agentY,
                                s_astar.goalX,  s_astar.goalY) >= 2) break;
    }

    // Fallback: open grid
    if (s_astar.path.size() < 2)
    {
        std::fill(s_astar.walls.begin(), s_astar.walls.end(), 0);
        astar_findpath_impl(s_astar.agentX, s_astar.agentY,
                            s_astar.goalX,  s_astar.goalY);
    }

    s_astar.wallsInitialized = true;
}


// Node: astar_retarget
// Inputs: (none)
// Output: void
// Description: Picks a random open cell as the new goal, then calls astar_findpath.
static void astar_retarget_impl()
{
    if (!s_astar.initialized) return;

    std::uniform_int_distribution<int> dx(0, s_astar.w - 1);
    std::uniform_int_distribution<int> dy(0, s_astar.h - 1);

    for (int attempt = 0; attempt < 100; ++attempt)
    {
        int nx = dx(s_astar.rng), ny = dy(s_astar.rng);
        if (nx == s_astar.agentX && ny == s_astar.agentY) continue;
        if (s_astar.walls[CellIdx(nx, ny)]) continue;

        s_astar.goalX = nx;
        s_astar.goalY = ny;
        if (astar_findpath_impl(s_astar.agentX, s_astar.agentY, nx, ny) >= 2) return;
    }
}


// Node: astar_init
// Inputs: gridW (number), gridH (number), startX (number), startY (number), goalX (number), goalY (number)
// Output: void
// Description: Sets grid dimensions and starting positions, then delegates to sub-nodes.
static void astar_init_impl(double gridW, double gridH,
                            double startX, double startY,
                            double goalX,  double goalY)
{
    if (!GetNativeRenderPanel()) return;

    s_astar.w      = std::max(2, ToInt(gridW));
    s_astar.h      = std::max(2, ToInt(gridH));
    s_astar.agentX = std::clamp(ToInt(startX), 0, s_astar.w - 1);
    s_astar.agentY = std::clamp(ToInt(startY), 0, s_astar.h - 1);
    s_astar.goalX  = std::clamp(ToInt(goalX),  0, s_astar.w - 1);
    s_astar.goalY  = std::clamp(ToInt(goalY),  0, s_astar.h - 1);

    if (!s_astar.wallsInitialized || (int)s_astar.walls.size() != s_astar.w * s_astar.h)
    {
        s_astar.wallsInitialized = false;
        astar_initwalls_impl();                                     // Phase 6: graph loop
    }

    s_astar.walls[CellIdx(s_astar.agentX, s_astar.agentY)] = 0;
    s_astar.walls[CellIdx(s_astar.goalX,  s_astar.goalY)]  = 0;
    s_astar.initialized = true;

    astar_findpath_impl(s_astar.agentX, s_astar.agentY,
                        s_astar.goalX,  s_astar.goalY);             // Phase 7: graph

    if (s_astar.path.size() < 2) astar_retarget_impl();             // Phase 5: graph
}


// Node: astar_step
// Inputs: (none)
// Output: void
// Description: Moves agent one cell along the path; calls astar_retarget on arrival.
static void astar_step_impl()
{
    if (!GetNativeRenderPanel() || !s_astar.initialized) return;

    if (s_astar.pathStep < s_astar.path.size())
    {
        s_astar.agentX = s_astar.path[s_astar.pathStep].x;
        s_astar.agentY = s_astar.path[s_astar.pathStep].y;
        ++s_astar.pathStep;
        return;
    }

    astar_retarget_impl();                                          // Phase 5: graph
}


// Node: astar_get
// Inputs: key (number)
// Output: number
// Description: key 0 = agentX, 1 = agentY, 2 = goalX, 3 = goalY
static double astar_get_impl(double key)
{
    if (!s_astar.initialized) return 0.0;
    switch (ToInt(key))
    {
        case 0: return (double)s_astar.agentX;
        case 1: return (double)s_astar.agentY;
        case 2: return (double)s_astar.goalX;
        case 3: return (double)s_astar.goalY;
        default: return 0.0;
    }
}


// Node: astar_getwall
// Inputs: x (number), y (number)
// Output: number (1 = wall, 0 = open)
static double astar_getwall_impl(double x, double y)
{
    if (!s_astar.initialized) return 0.0;
    const int ix = std::clamp(ToInt(x), 0, s_astar.w - 1);
    const int iy = std::clamp(ToInt(y), 0, s_astar.h - 1);
    return (double)s_astar.walls[CellIdx(ix, iy)];
}


// Node: astar_setwall
// Inputs: x (number), y (number), isWall (number)
// Output: void
// Description: Writes wall state, recomputes path via astar_findpath, retargets if blocked.
static void astar_setwall_impl(double x, double y, double isWall)
{
    if (!s_astar.initialized) return;

    const int ix = std::clamp(ToInt(x), 0, s_astar.w - 1);
    const int iy = std::clamp(ToInt(y), 0, s_astar.h - 1);
    if (ix == s_astar.agentX && iy == s_astar.agentY) return;

    const int     i      = CellIdx(ix, iy);
    const uint8_t newVal = (isWall != 0.0) ? 1 : 0;
    if (s_astar.walls[i] == newVal) return;

    s_astar.walls[i] = newVal;

    astar_findpath_impl(s_astar.agentX, s_astar.agentY,
                        s_astar.goalX,  s_astar.goalY);             // Phase 7: graph

    if (s_astar.path.size() < 2) astar_retarget_impl();             // Phase 5: graph
}


// Node: astar_getpathlen
// Inputs: (none)
// Output: number
// Description: Returns remaining path steps (path.size - pathStep).
static double astar_getpathlen_impl()
{
    if (!s_astar.initialized) return 0.0;
    return (s_astar.pathStep < s_astar.path.size())
        ? (double)(s_astar.path.size() - s_astar.pathStep)
        : 0.0;
}


// Node: astar_getpathcell_x
// Inputs: index (number)
// Output: number
// Description: Read X of path[pathStep + index].
static double astar_getpathcell_x_impl(double index)
{
    if (!s_astar.initialized) return 0.0;
    const size_t i = s_astar.pathStep + (size_t)std::max(0LL, std::llround(index));
    return (i < s_astar.path.size()) ? (double)s_astar.path[i].x : 0.0;
}

// Node: astar_getpathcell_y
// Inputs: index (number)
// Output: number
// Description: Read Y of path[pathStep + index].
static double astar_getpathcell_y_impl(double index)
{
    if (!s_astar.initialized) return 0.0;
    const size_t i = s_astar.pathStep + (size_t)std::max(0LL, std::llround(index));
    return (i < s_astar.path.size()) ? (double)s_astar.path[i].y : 0.0;
}


// Node: astar_setagent
// Inputs: x (number), y (number)
// Output: void
// Description: Write agent position without recomputing path.
static void astar_setagent_impl(double x, double y)
{
    if (!s_astar.initialized) return;
    s_astar.agentX = std::clamp(ToInt(x), 0, s_astar.w - 1);
    s_astar.agentY = std::clamp(ToInt(y), 0, s_astar.h - 1);
}


// Node: astar_setpathstep
// Inputs: step (number)
// Output: void
// Description: Write the current path index directly.
static void astar_setpathstep_impl(double step)
{
    if (!s_astar.initialized) return;
    s_astar.pathStep = (size_t)std::max(0LL, std::llround(step));
}



// -----------------------------------------------------------------------------
// EMI registrations — primitives live in NodeGameFunctions.cpp
// -----------------------------------------------------------------------------

EMI_REGISTER(astar_findpath,      astar_findpath_impl)
EMI_REGISTER(astar_initwalls,     astar_initwalls_impl)
EMI_REGISTER(astar_retarget,      astar_retarget_impl)
EMI_REGISTER(astar_init,          astar_init_impl)
EMI_REGISTER(astar_step,          astar_step_impl)
EMI_REGISTER(astar_get,           astar_get_impl)
EMI_REGISTER(astar_getwall,       astar_getwall_impl)
EMI_REGISTER(astar_setwall,       astar_setwall_impl)
EMI_REGISTER(astar_getpathlen,    astar_getpathlen_impl)
EMI_REGISTER(astar_getpathcell_x, astar_getpathcell_x_impl)
EMI_REGISTER(astar_getpathcell_y, astar_getpathcell_y_impl)
EMI_REGISTER(astar_setagent,      astar_setagent_impl)
EMI_REGISTER(astar_setpathstep,   astar_setpathstep_impl)

// Used by compiler-emitted graphs (Ticker/Delay) to allow Force Stop to break
// out even while sleeping.
EMI_REGISTER(__emi_force_stop_requested, __emi_force_stop_requested_impl)
EMI_REGISTER(__emi_delay, __emi_delay_impl)
