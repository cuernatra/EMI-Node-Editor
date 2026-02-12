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

struct Node
{
    ed::NodeId id;
    std::string name;

    std::vector<ed::PinId> pins;
};

struct Link
{
    ed::LinkId id;
    ed::PinId  startPinId; // output
    ed::PinId  endPinId;   // input
};

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

    std::vector<Node> m_nodes;
    std::vector<Link> m_links;

    void saveGraph(const char* path) const;
    void loadGraph(const char* path);
};

#endif