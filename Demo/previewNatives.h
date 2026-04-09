#pragma once
// A* pathfinder helpers for the node-editor demo.
// SetRenderPanelForNatives / ShowRenderPanelButton are declared in NodeGameFunctions.h.

#include <atomic>

// Wiring hook: GraphExecutor provides its stop flag so demo/runtime natives can
// implement interruptible delays and stop checks for long-running graphs.
void SetGraphForceStopFlag(std::atomic<bool>* flag);
