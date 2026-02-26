#include "visualNode.h"

VisualNode CreateVisualNode(IdGen& gen, std::string title, ImVec2 pos, std::string type, std::string value)
{
    VisualNode n;
    n.id = gen.NewNode();
    n.inPin  = gen.NewPin();
    n.outPin = gen.NewPin();
    n.title = std::move(title);
    n.type = std::move(type);
    n.value = std::move(value);
    n.initialPos = pos;
    return n;
}

VisualNode CreateVisualNodeWithId(int nodeId, int inPinId, int outPinId,
                                  std::string title, ImVec2 pos, std::string type, std::string value)
{
    VisualNode n;
    n.id = ed::NodeId(nodeId);
    n.inPin = ed::PinId(inPinId);
    n.outPin = ed::PinId(outPinId);
    n.title = std::move(title);
    n.type = std::move(type);
    n.value = std::move(value);
    n.initialPos = pos;
    
    n.positioned = true;
    return n;
}

void DrawVisualNode(VisualNode& n)
{
    if (!n.positioned) 
    {
        ed::SetNodePosition(n.id, n.initialPos);
        n.positioned = true;
    }

    ed::BeginNode(n.id);

    ImGui::TextUnformatted(n.title.c_str());

    ed::BeginPin(n.inPin, ed::PinKind::Input);
    ImGui::Text("-> In");
    ed::EndPin();

    ed::BeginPin(n.outPin, ed::PinKind::Output);
    ImGui::Text("Out ->");
    ed::EndPin();

    ed::EndNode();
}