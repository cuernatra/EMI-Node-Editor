#include <catch2/catch_test_macros.hpp>

#include "editor/graph/graphState.h"

namespace
{
VisualNode MakeNode(int nodeId, NodeType type, std::initializer_list<int> inPins, std::initializer_list<int> outPins)
{
    VisualNode n;
    n.id = ed::NodeId(nodeId);
    n.nodeType = type;

    for (int pinId : inPins)
    {
        Pin p;
        p.id = ed::PinId(pinId);
        p.parentNodeId = n.id;
        p.parentNodeType = type;
        p.isInput = true;
        n.inPins.push_back(p);
    }

    for (int pinId : outPins)
    {
        Pin p;
        p.id = ed::PinId(pinId);
        p.parentNodeId = n.id;
        p.parentNodeType = type;
        p.isInput = false;
        n.outPins.push_back(p);
    }

    return n;
}
}

TEST_CASE("GraphState deletes links connected to a deleted node", "[graph]")
{
    GraphState state;

    state.AddNode(MakeNode(1, NodeType::Start, {}, {11}));
    state.AddNode(MakeNode(2, NodeType::Output, {22}, {}));

    Link l;
    l.id = ed::LinkId(99);
    l.startPinId = ed::PinId(11);
    l.endPinId = ed::PinId(22);
    l.type = PinType::Flow;
    state.AddLink(l);

    REQUIRE(state.GetLinks().size() == 1);

    state.DeleteNode(ed::NodeId(1));

    REQUIRE(state.GetLinks().empty());
    REQUIRE_FALSE(state.GetNodes()[0].alive);
}

TEST_CASE("GrapghState removes link when DeleteLink is called", "[graph]")
{
    GraphState state;

    state.AddNode(MakeNode(1, NodeType::Start, {}, {1}));
    state.AddNode(MakeNode(2, NodeType::Output, {2}, {}));

    Link link;
    link.id = ed::LinkId(3);
    link.startPinId = ed::PinId(1);
    link.endPinId = ed::PinId(2);
    link.type = PinType::Flow;
    state.AddLink(link);

    REQUIRE(state.GetLinks().size() == 1);
    
    state.DeleteLink(link.id);

    REQUIRE(state.GetLinks().empty());
    REQUIRE(state.GetNodes().size() == 2);
}
