#ifndef VISUALNODE_H
#define VISUALNODE_H

#include "imgui_node_editor.h"
#include <vector>
#include <string>

namespace ed = ax::NodeEditor;

struct IdGen
{
    int next = 1;

    void SetNext(int v)
    {
        next = v;
    }

    ed::NodeId NewNode() { return ed::NodeId(next++); }
    ed::PinId  NewPin()  { return ed::PinId(next++); }
    ed::LinkId NewLink() { return ed::LinkId(next++); }
};

struct VisualNode 
{
    ed::NodeId id{};
    ed::PinId  inPin{};
    ed::PinId  outPin{};
    std::string title;
    std::string type;
    std::string value;
    ImVec2 initialPos{0,0};
    bool positioned = false;
    bool alive = true;
};

struct Link
{
    ed::LinkId id;
    ed::PinId  startPinId; // output
    ed::PinId  endPinId;   // input
};

struct NodeSpawnPayload
{
    char title[32];
};

VisualNode CreateVisualNode(IdGen& gen, std::string title, ImVec2 pos, std::string type, std::string value);
void DrawVisualNode(VisualNode& n);
VisualNode CreateVisualNodeWithId(int nodeId, int inPinId, int outPinId,
                                  std::string title, ImVec2 pos, std::string type, std::string value);
#endif