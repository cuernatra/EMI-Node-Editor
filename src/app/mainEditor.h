#ifndef MAINEDITOR_H
#define MAINEDITOR_H

#include "constants.h"
#include "imgui_node_editor.h"
#include <vector>
#include <string>

/**
 * @brief Represent node-editor.
 * Uses imgui_node_editor library for node editor functionality.
 *
 * @author Joel Turkka
 */
namespace ed = ax::NodeEditor;

struct IdGen {
    int next = 1;

    ed::NodeId NewNode() { return ed::NodeId(next++); }
    ed::PinId  NewPin()  { return ed::PinId(next++); }
    ed::LinkId NewLink() { return ed::LinkId(next++); }
};

struct SimpleNode {
    ed::NodeId id{};
    ed::PinId  inPin{};
    ed::PinId  outPin{};
    std::string title;
    ImVec2 initialPos{0,0};
    bool positioned = false;
    bool alive = true;
};

// struct Node
// {
//     ed::NodeId id;
//     std::string name;

//     std::vector<ed::PinId> pins;
// };

struct Link
{
    ed::LinkId id;
    ed::PinId  startPinId; // output
    ed::PinId  endPinId;   // input
};

SimpleNode CreateSimpleNode(IdGen& gen, std::string title, ImVec2 pos);
void DrawSimpleNode(SimpleNode& n);

class MainEditor
{
public:
    MainEditor();
    ~MainEditor();
    void draw();

private:
    ax::NodeEditor::EditorContext* m_ctx = nullptr;
    bool m_firstFrame = true;

    int m_nextLinkId = 100;

    // std::vector<Node> m_nodes;
    std::vector<Link> m_links;

    IdGen gen;
    std::vector<SimpleNode> m_nodes;
    
    void saveGraph(const char* path) const;
    void loadGraph(const char* path);
    void removeLinksForNode(const SimpleNode& n);
};

#endif