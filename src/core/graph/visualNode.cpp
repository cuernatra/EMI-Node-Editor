#include "visualNode.h"
#include "../registry/fieldWidget.h"
#include "imgui_node_editor.h"

static void DrawPin(const Pin& pin)
{
    ed::BeginPin(pin.id,
                 pin.isInput ? ed::PinKind::Input : ed::PinKind::Output);

    ImVec4 col = pin.GetTypeColor();
    ImGui::PushStyleColor(ImGuiCol_Text, col);

    if (pin.isInput)
        ImGui::Text("-> %s", pin.name.c_str());
    else
        ImGui::Text("%s ->", pin.name.c_str());

    ImGui::PopStyleColor();

    ed::EndPin();
}

bool DrawVisualNode(VisualNode& n)
{
    if (!n.positioned)
    {
        ed::SetNodePosition(n.id, n.initialPos);
        n.positioned = true;
    }

     ed::BeginNode(n.id);

    ImGui::TextUnformatted(n.title.c_str());
    ImGui::Spacing();

    for (const Pin& pin : n.inPins)
        DrawPin(pin);

    bool changed = false;
    if (!n.fields.empty())
    {
        ImGui::Spacing();
        ImGui::PushID((int)n.id.Get());
        for (NodeField& field : n.fields)
            changed |= DrawField(field);
        ImGui::PopID();
        ImGui::Spacing();
    }

    for (const Pin& pin : n.outPins)
        DrawPin(pin);

    ed::EndNode();
    return changed;
}

