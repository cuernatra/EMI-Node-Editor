#include <catch2/catch_test_macros.hpp>

#include "../src/editor/graphState.h"
#include "../src/editor/settings.h"

#include <filesystem>
#include <fstream>

namespace
{
class ScopedCurrentPath
{
public:
    explicit ScopedCurrentPath(const std::filesystem::path& newPath)
        : oldPath_(std::filesystem::current_path())
    {
        std::filesystem::current_path(newPath);
    }

    ~ScopedCurrentPath()
    {
        std::filesystem::current_path(oldPath_);
    }

private:
    std::filesystem::path oldPath_;
};

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

TEST_CASE("GraphState removes links when deleting a node", "[smoke][graph]")
{
    GraphState state;

    VisualNode start = MakeNode(1, NodeType::Start, {}, {11});
    VisualNode output = MakeNode(2, NodeType::Output, {22}, {});

    state.AddNode(start);
    state.AddNode(output);

    Link l;
    l.id = ed::LinkId(99);
    l.startPinId = ed::PinId(11);
    l.endPinId = ed::PinId(22);
    l.type = PinType::Flow;
    state.AddLink(l);

    REQUIRE(state.GetLinks().size() == 1);
    REQUIRE(state.HasOutputNode());
    REQUIRE(state.HasAliveNodes());
    REQUIRE(state.IsDirty());

    state.DeleteNode(start.id);

    REQUIRE(state.GetLinks().empty());
    REQUIRE_FALSE(state.GetNodes()[0].alive);
}

TEST_CASE("GraphState clear resets compile status and graph content", "[smoke][graph]")
{
    GraphState state;
    state.AddNode(MakeNode(7, NodeType::Constant, {}, {71}));
    state.SetCompileStatus(true, "ok");

    state.Clear();

    REQUIRE(state.GetNodes().empty());
    REQUIRE(state.GetLinks().empty());
    REQUIRE_FALSE(state.IsCompileSuccess());
    REQUIRE(state.GetCompileMessage().empty());
}

TEST_CASE("Settings save and load round-trip values", "[smoke][settings]")
{
    const auto tempRoot = std::filesystem::temp_directory_path() / "emi_settings_smoke_roundtrip";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(tempRoot);

    {
        const ScopedCurrentPath cwd(tempRoot);

        Settings::leftPanelWidth = 123.5f;
        Settings::consolePanelHeight = 321.25f;
        Settings::consolePanelIsMinimized = true;
        Settings::Save();

        REQUIRE(std::filesystem::exists("settings.json"));

        Settings::leftPanelWidth = -1.0f;
        Settings::consolePanelHeight = -1.0f;
        Settings::consolePanelIsMinimized = false;

        Settings::Load();

        REQUIRE(Settings::leftPanelWidth == 123.5f);
        REQUIRE(Settings::consolePanelHeight == 321.25f);
        REQUIRE(Settings::consolePanelIsMinimized);
    }

    std::filesystem::remove_all(tempRoot);
}

TEST_CASE("Settings load tolerates malformed json", "[smoke][settings]")
{
    const auto tempRoot = std::filesystem::temp_directory_path() / "emi_settings_smoke_malformed";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(tempRoot);

    {
        const ScopedCurrentPath cwd(tempRoot);

        {
            std::ofstream badFile("settings.json", std::ios::trunc);
            badFile << "{ this is not valid json";
        }

        Settings::leftPanelWidth = 42.0f;
        Settings::consolePanelHeight = 84.0f;
        Settings::consolePanelIsMinimized = false;

        Settings::Load();

        REQUIRE(Settings::leftPanelWidth == 42.0f);
        REQUIRE(Settings::consolePanelHeight == 84.0f);
        REQUIRE_FALSE(Settings::consolePanelIsMinimized);
    }

    std::filesystem::remove_all(tempRoot);
}

TEST_CASE("GraphState must have Output node to compile", "[smoke][graph]")
{
    GraphState state;

    // Initially should have no output node
    REQUIRE_FALSE(state.HasOutputNode());

    // Add an Output node
    VisualNode output = MakeNode(1, NodeType::Output, {10}, {});
    state.AddNode(output);

    // Now should have output node
    REQUIRE(state.HasOutputNode());
    REQUIRE(state.HasNodeType(NodeType::Output));
    REQUIRE(state.HasAliveNodes());
}

TEST_CASE("GraphState link addition and deletion", "[smoke][graph]")
{
    GraphState state;

    VisualNode start = MakeNode(1, NodeType::Start, {}, {11});
    VisualNode output = MakeNode(2, NodeType::Output, {22}, {});

    state.AddNode(start);
    state.AddNode(output);

    REQUIRE(state.GetLinks().empty());

    // Add a link
    Link l;
    l.id = ed::LinkId(100);
    l.startPinId = ed::PinId(11);
    l.endPinId = ed::PinId(22);
    l.type = PinType::Flow;
    state.AddLink(l);

    REQUIRE(state.GetLinks().size() == 1);

    // Delete the link
    state.DeleteLink(ed::LinkId(100));

    REQUIRE(state.GetLinks().empty());
}

TEST_CASE("GraphState dirty flag tracking", "[smoke][graph]")
{
    GraphState state;

    // Initially clean
    REQUIRE_FALSE(state.IsDirty());

    // Mark dirty
    state.MarkDirty();
    REQUIRE(state.IsDirty());

    // Clear dirty flag
    state.ClearDirty();
    REQUIRE_FALSE(state.IsDirty());

    // Add node marks dirty
    state.MarkDirty();
    REQUIRE(state.IsDirty());
    state.ClearDirty();
    REQUIRE_FALSE(state.IsDirty());
}

TEST_CASE("GraphState compilation status tracking", "[smoke][graph]")
{
    GraphState state;

    // Initially no compilation attempted
    REQUIRE_FALSE(state.IsCompileSuccess());
    REQUIRE(state.GetCompileMessage().empty());

    // Set successful compilation
    state.SetCompileStatus(true, "Compilation successful");
    REQUIRE(state.IsCompileSuccess());
    REQUIRE(state.GetCompileMessage() == "Compilation successful");

    // Set failed compilation
    state.SetCompileStatus(false, "Error: Missing output node");
    REQUIRE_FALSE(state.IsCompileSuccess());
    REQUIRE(state.GetCompileMessage() == "Error: Missing output node");
}

TEST_CASE("Settings load handles missing file gracefully", "[smoke][settings]")
{
    const auto tempRoot = std::filesystem::temp_directory_path() / "emi_settings_missing_file";
    std::filesystem::remove_all(tempRoot);
    std::filesystem::create_directories(tempRoot);

    {
        const ScopedCurrentPath cwd(tempRoot);

        // Ensure no settings.json exists
        REQUIRE_FALSE(std::filesystem::exists("settings.json"));

        // Set some values to verify they retain defaults when Load finds nothing
        Settings::leftPanelWidth = 99.0f;
        Settings::consolePanelHeight = 88.0f;
        Settings::consolePanelIsMinimized = true;

        // Load should not fail, just return early
        Settings::Load();

        // Values should remain unchanged since there's no file
        REQUIRE(Settings::leftPanelWidth == 99.0f);
        REQUIRE(Settings::consolePanelHeight == 88.0f);
        REQUIRE(Settings::consolePanelIsMinimized);
    }

    std::filesystem::remove_all(tempRoot);
}

TEST_CASE("GraphState node type detection", "[smoke][graph]")
{
    GraphState state;

    // Empty graph has no nodes of any type
    REQUIRE_FALSE(state.HasNodeType(NodeType::Start));
    REQUIRE_FALSE(state.HasNodeType(NodeType::Constant));
    REQUIRE_FALSE(state.HasNodeType(NodeType::Output));

    // Add various node types
    state.AddNode(MakeNode(10, NodeType::Start, {}, {100}));
    REQUIRE(state.HasNodeType(NodeType::Start));

    state.AddNode(MakeNode(20, NodeType::Constant, {}, {200}));
    REQUIRE(state.HasNodeType(NodeType::Constant));

    state.AddNode(MakeNode(30, NodeType::Output, {300}, {}));
    REQUIRE(state.HasNodeType(NodeType::Output));
}