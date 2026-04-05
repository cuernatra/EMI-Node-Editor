#include "NodeGameFunctions.h"
#include "editor/panels/renderPanel.h"
#include <EMI/EMI.h>
#include <Defines.h>
#include <algorithm>
#include <cmath>
#include <random>

// Pointer to the live render panel, set once before any VM execution.
static RenderPanel* s_renderPanel = nullptr;

void SetRenderPanelForNatives(RenderPanel* panel)
{
    s_renderPanel = panel;
}

RenderPanel* GetNativeRenderPanel()
{
    return s_renderPanel;
}

void ShowRenderPanelButton()
{
    // Stub — UI layer should call: if (ImGui::Button("Open Render Panel")) s_renderPanel->open();
}

// ---------------------------------------------------------------------------
// Drawing — equivalents of writepixel / render / setupconsole
// ---------------------------------------------------------------------------

static void drawcell_impl(double x, double y, double r, double g, double b)
{
    if (s_renderPanel)
        s_renderPanel->drawCell(
            static_cast<int>(x), static_cast<int>(y),
            static_cast<int>(r), static_cast<int>(g), static_cast<int>(b));
}

static void cleargrid_impl(double w, double h)
{
    if (s_renderPanel)
        s_renderPanel->clearGrid(static_cast<int>(w), static_cast<int>(h));
}

static void rendergrid_impl()
{
    if (s_renderPanel)
        s_renderPanel->renderGrid();
}

// ---------------------------------------------------------------------------
// Pixel read-back — equivalent of Game.GetPixel
// ---------------------------------------------------------------------------

static double getpixel_impl(double x, double y)
{
    if (!s_renderPanel) return 0.0;
    return static_cast<double>(s_renderPanel->getCell(static_cast<int>(x), static_cast<int>(y)));
}

// ---------------------------------------------------------------------------
// Input — equivalents of Input.WaitInput / IsKeyDown / IsMouseButtonDown / GetMouseX/Y
// ---------------------------------------------------------------------------

static double waitinput_impl()
{
    if (!s_renderPanel) return -1.0;
    return static_cast<double>(s_renderPanel->waitInput());
}

static double iskeydown_impl(double keycode)
{
    if (!s_renderPanel) return 0.0;
    return s_renderPanel->isKeyDown(static_cast<int>(keycode)) ? 1.0 : 0.0;
}

static double ismousebuttondown_impl(double button)
{
    if (!s_renderPanel) return 0.0;
    return s_renderPanel->isMouseButtonDown(static_cast<int>(button)) ? 1.0 : 0.0;
}

static double getmousex_impl()
{
    if (!s_renderPanel) return 0.0;
    return static_cast<double>(s_renderPanel->getMouseX());
}

static double getmousey_impl()
{
    if (!s_renderPanel) return 0.0;
    return static_cast<double>(s_renderPanel->getMouseY());
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

static double random_impl(double lo, double hi)
{
    const int minValue = static_cast<int>(std::llround(std::min(lo, hi)));
    const int maxValue = static_cast<int>(std::llround(std::max(lo, hi)));
    static thread_local std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dist(minValue, maxValue);
    return static_cast<double>(dist(rng));
}

// ---------------------------------------------------------------------------
// EMI registrations — names match the original GameFunctions.cpp
// ---------------------------------------------------------------------------

EMI_REGISTER(setupconsole,             cleargrid_impl)
EMI_REGISTER(writepixel,               drawcell_impl)
EMI_REGISTER(render,                   rendergrid_impl)
EMI_REGISTER(Game.GetPixel,            getpixel_impl)
EMI_REGISTER(Input.WaitInput,          waitinput_impl)
EMI_REGISTER(Input.IsKeyDown,          iskeydown_impl)
EMI_REGISTER(Input.IsMouseButtonDown,  ismousebuttondown_impl)
EMI_REGISTER(Input.GetMouseX,          getmousex_impl)
EMI_REGISTER(Input.GetMouseY,          getmousey_impl)
EMI_REGISTER(random,                   random_impl)

// Also register under the node-editor names used by the current demo graph.
EMI_REGISTER(drawcell,                 drawcell_impl)
EMI_REGISTER(cleargrid,                cleargrid_impl)
EMI_REGISTER(rendergrid,               rendergrid_impl)
