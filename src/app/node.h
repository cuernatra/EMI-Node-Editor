#ifndef NODE_H
#define NODE_H

#include "imgui_node_editor.h"
#include <vector>
#include <string>

namespace ed = ax::NodeEditor;

struct IdGen 
{
    int next = 1;
    ed::NodeId NewNode() { return ed::NodeId(next++); }
    ed::PinId  NewPin()  { return ed::PinId(next++); }
    ed::LinkId NewLink() { return ed::LinkId(next++); }
};

struct SimpleNode 
{
    ed::NodeId id{};
    ed::PinId  inPin{};
    ed::PinId  outPin{};
    std::string title;
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

SimpleNode CreateSimpleNode(IdGen& gen, std::string title, ImVec2 pos);
void DrawSimpleNode(SimpleNode& n);

#endif