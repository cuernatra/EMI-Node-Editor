#include "graphState.h"
#include <algorithm>

GraphState::GraphState()
{
}

void GraphState::Clear()
{
    m_nodes.clear();
    m_links.clear();
    m_compileMessage.clear();
    m_compileSuccess = false;
}

void GraphState::AddNode(const VisualNode& node)
{
    m_nodes.push_back(node);
    m_dirty = true;
}

void GraphState::DeleteNode(ed::NodeId nodeId)
{
    auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
        [&](const VisualNode& n) { return n.alive && n.id == nodeId; });

    if (it != m_nodes.end())
    {
        RemoveLinksForNode(*it);
        it->alive = false;
        m_dirty = true;
    }
}

const Pin* GraphState::FindPin(ed::PinId pinId) const
{
    for (const auto& n : m_nodes)
    {
        if (!n.alive) continue;
        for (const Pin& p : n.inPins)
            if (p.id == pinId) return &p;
        for (const Pin& p : n.outPins)
            if (p.id == pinId) return &p;
    }
    return nullptr;
}

void GraphState::RemoveLinksForNode(const VisualNode& n)
{
    std::vector<ed::PinId> allPins;
    allPins.reserve(n.inPins.size() + n.outPins.size());
    for (const Pin& p : n.inPins)  allPins.push_back(p.id);
    for (const Pin& p : n.outPins) allPins.push_back(p.id);

    auto pinBelongsToNode = [&](ed::PinId id) {
        for (ed::PinId p : allPins)
            if (p == id) return true;
        return false;
    };

    auto it = std::remove_if(m_links.begin(), m_links.end(),
        [&](const Link& l) {
            return pinBelongsToNode(l.startPinId) ||
                   pinBelongsToNode(l.endPinId);
        });

    if (it != m_links.end())
        m_links.erase(it, m_links.end());
}

void GraphState::AddLink(const Link& link)
{
    m_links.push_back(link);
    m_dirty = true;
}

void GraphState::DeleteLink(ed::LinkId linkId)
{
    auto it = std::remove_if(m_links.begin(), m_links.end(),
        [&](const Link& l) { return l.id == linkId; });

    if (it != m_links.end())
        m_links.erase(it, m_links.end());
    m_dirty = true;
}

void GraphState::SetCompileStatus(bool success, const std::string& message)
{
    m_compileSuccess = success;
    m_compileMessage = message;
}

bool GraphState::HasOutputNode() const
{
    for (const auto& n : m_nodes)
        if (n.alive && n.nodeType == NodeType::Output)
            return true;
    return false;
}

bool GraphState::HasNodeType(NodeType type) const
{
    for (const auto& n : m_nodes)
        if (n.alive && n.nodeType == type)
            return true;
    return false;
}

bool GraphState::HasAliveNodes() const
{
    for (const auto& n : m_nodes)
        if (n.alive)
            return true;
    return false;
}
