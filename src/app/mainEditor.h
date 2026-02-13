#ifndef MAINEDITOR_H
#define MAINEDITOR_H

#include "constants.h"
#include "imgui_node_editor.h"
#include "node.h"
#include <vector>
#include <string>

/**
 * @brief Represent node-editor.
 * Uses imgui_node_editor library for node editor functionality.
 *
 * @author Joel Turkka
 */

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

    std::vector<Link> m_links;

    IdGen gen;
    std::vector<SimpleNode> m_nodes;
    
    void saveGraph(const char* path) const;
    void loadGraph(const char* path);
    void removeLinksForNode(const SimpleNode& n);
    void createNewLink();
    void deleteLinks(ed::LinkId linkId);
    void deleteNodes(ed::NodeId nodeId);
};

#endif