#include <catch2/catch_test_macros.hpp>

// NOTE: Current architecture couples data structures to ImGui/imgui-node-editor,
// making unit testing of core logic require refactoring data types to not depend
// on them (e.g. using plain int instead of ed::NodeId/ed::PinId).
// This is a known architectural limitation.

TEST_CASE("Placeholder - CI pipeline verification", "[placeholder]") {
    REQUIRE(1 == 1);
}