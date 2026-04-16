#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <sys/types.h>
#include <vector>
#include "core/graphModel/pin.h"
#include "core/graphModel/link.h"
#include "core/graphModel/types.h"
#include "imgui_node_editor.h"

namespace
{
Pin MakeTestPin(uint32_t id, PinType type, bool isInput)
{
    return MakePin(id, ed::NodeId(100), NodeType::Constant, "test", type, isInput);
}

Link MakeTestLink(uint32_t id, uint32_t startPin, uint32_t endPin)
{
    Link l;
    l.id = ed::LinkId(id);
    l.startPinId = ed::PinId(startPin);
    l.endPinId = ed::PinId(endPin);
    l.type = PinType::Flow;
    l.alive = true;
    return l;
}
}

TEST_CASE("No cycles in empty graph", "[link]")
{
    std::vector<Link> links;
    REQUIRE_FALSE(WouldCreateCycle(links, ed::PinId(1), ed::PinId(2)));
}

TEST_CASE("No self reference", "[link]")
{
    std::vector<Link> links;
    REQUIRE(WouldCreateCycle(links, ed::PinId(1), ed::PinId(1)));
}

TEST_CASE("No cycle between two pins", "[link]")
{
    std::vector<Link> links = {MakeTestLink(99, 1, 2)};
    REQUIRE(WouldCreateCycle(links, ed::PinId(2), ed::PinId(1)));
}
