#include "link.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ---------------------------------------------------------------------------
// WouldCreateCycle
//
// Runs a DFS from 'startPin' (output) to see if we can reach the node that
// owns 'endPin' (input) by following existing links.  If we can, adding this
// new link would form a cycle.
//
// NOTE: This operates on pin IDs.  It assumes output pin -> input pin
// directionality mirrors node -> node edges.  For a heterogeneous graph you
// may want a separate node-level adjacency structure; this is sufficient for
// a standard expression/flow graph where each node has one output.
// ---------------------------------------------------------------------------

bool WouldCreateCycle(const std::vector<Link>& links,
                      ed::PinId startPin, ed::PinId endPin)
{
    // Build adjacency: outputPinId -> [inputPinIds]
    std::unordered_map<uintptr_t, std::vector<uintptr_t>> adj;
    for (const auto& lnk : links)
    {
        if (!lnk.alive) continue;
        adj[lnk.startPinId.Get()].push_back(lnk.endPinId.Get());
    }

    // DFS from endPin — if we reach startPin, a cycle would form.
    std::unordered_set<uintptr_t> visited;
    std::vector<uintptr_t> stack;
    stack.push_back(endPin.Get());

    while (!stack.empty())
    {
        uintptr_t current = stack.back();
        stack.pop_back();

        if (current == startPin.Get())
            return true;

        if (visited.count(current))
            continue;
        visited.insert(current);

        auto it = adj.find(current);
        if (it != adj.end())
            for (uintptr_t next : it->second)
                stack.push_back(next);
    }

    return false;
}

