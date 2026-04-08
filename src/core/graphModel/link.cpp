#include "link.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Cycle check used before adding a new link.
// If walking existing links from endPin can reach startPin,
// adding startPin -> endPin would close a loop.

bool WouldCreateCycle(const std::vector<Link>& links,
                      ed::PinId startPin, ed::PinId endPin)
{
    // Build quick lookup: output pin -> connected input pins.
    std::unordered_map<uintptr_t, std::vector<uintptr_t>> adj;
    for (const auto& lnk : links)
    {
        if (!lnk.alive) continue;
        adj[lnk.startPinId.Get()].push_back(lnk.endPinId.Get());
    }

    // Walk from endPin. Reaching startPin means a cycle.
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

