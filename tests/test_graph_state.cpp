#include <catch2/catch_test_macros.hpp>

#include "core/graphModel/types.h"
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

TEST_CASE("Pins connections work properly")
{
    // Same types connects
    Pin out = MakePin(1, ed::NodeId(10), NodeType::Constant, "Value", PinType::Number, false);
    Pin in = MakePin(2, ed::NodeId(11), NodeType::Output, "Value", PinType::Number, true);
    REQUIRE(Pin::CanConnect(out, in));
    
    // Different types don't connect
    Pin strOut = MakePin(3, ed::NodeId(12), NodeType::Constant, "Value", PinType::String, false);
    REQUIRE_FALSE(Pin::CanConnect(strOut, in));

    // Any type connects to any
    Pin anyIn = MakePin(4, ed::NodeId(13), NodeType::Output, "Value", PinType::Any, true);
    REQUIRE(Pin::CanConnect(strOut, anyIn));

    // Input and output are backwards
    REQUIRE_FALSE(Pin::CanConnect(in, out));
}
