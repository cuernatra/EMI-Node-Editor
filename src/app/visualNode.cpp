#include "visualNode.h"

VisualNode CreateVisualNode(IdGen& gen, std::string title, ImVec2 pos,
                            std::string type, std::string value,
                            int inputCount)
{
    VisualNode n;
    n.id = gen.NewNode();

    if (inputCount < 1) inputCount = 1;
    n.inPins.reserve(inputCount);
    for (int i = 0; i < inputCount; ++i)
        n.inPins.push_back(gen.NewPin());

    n.outPin = gen.NewPin();
    n.title = std::move(title);
    n.type  = std::move(type);
    n.value = std::move(value);
    n.initialPos = pos;
    return n;
}

VisualNode CreateVisualNodeWithId(int nodeId,
                                  const std::vector<int>& inputPinIds,
                                  int outPinId,
                                  std::string title, ImVec2 pos,
                                  std::string type, std::string value)
{
    VisualNode n;
    n.id = ed::NodeId(nodeId);

    n.inPins.reserve(inputPinIds.size());
    for (int pid : inputPinIds)
        n.inPins.push_back(ed::PinId(pid));

    n.outPin = ed::PinId(outPinId);

    n.title = std::move(title);
    n.type  = std::move(type);
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

    for (int i = 0; i < (int)n.inPins.size(); ++i)
    {
        ed::BeginPin(n.inPins[i], ed::PinKind::Input);
        ImGui::Text("-> In %d", i + 1);
        ed::EndPin();
    }

    ed::BeginPin(n.outPin, ed::PinKind::Output);
    ImGui::Text("Out ->");
    ed::EndPin();

    ed::EndNode();
}