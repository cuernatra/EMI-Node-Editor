#include "graphEditor.h"
#include "graphState.h"
#include "graphSerializer.h"
#include "../core/graph/visualNode.h"
#include "../core/graph/link.h"
#include "../core/registry/nodeFactory.h"
#include "imgui.h"
#include <algorithm>
#include <string>
#include <vector>

namespace
{
const char* PinTypeToString(PinType t)
{
    switch (t)
    {
        case PinType::Number:   return "Number";
        case PinType::Boolean:  return "Boolean";
        case PinType::String:   return "String";
        case PinType::Array:    return "Array";
        case PinType::Function: return "Function";
        case PinType::Flow:     return "Flow";
        case PinType::Any:      return "Any";
        default:                return "Unknown";
    }
}
}

GraphEditor::GraphEditor(ed::EditorContext* ctx, GraphState& state)
    : m_ctx(ctx), m_state(state)
{
}

GraphEditor::~GraphEditor()
{
}

void GraphEditor::Draw()
{
    ed::SetCurrentEditor(m_ctx);

    DrawNodeCanvas();
    DrawContextMenus();

    ed::SetCurrentEditor(nullptr);
}

bool GraphEditor::HasSelectedNode() const
{
    ed::SetCurrentEditor(m_ctx);
    const bool hasSelected = (GetPrimarySelectedNode() != nullptr);
    ed::SetCurrentEditor(nullptr);
    return hasSelected;
}

void GraphEditor::DrawInspector()
{
    ed::SetCurrentEditor(m_ctx);

    ImGui::TextUnformatted("INSPECTOR");
    ImGui::Separator();

    const VisualNode* node = GetPrimarySelectedNode();
    if (!node)
    {
        ed::SetCurrentEditor(nullptr);
        return;
    }

    const ImVec2 pos = ed::GetNodePosition(node->id);

    ImGui::Text("ID: %d", static_cast<int>(node->id.Get()));
    ImGui::Text("Type: %s", NodeTypeToString(node->nodeType));
    ImGui::Text("Position: (%.1f, %.1f)", pos.x, pos.y);
    ImGui::Text("Inputs: %d", static_cast<int>(node->inPins.size()));
    ImGui::Text("Outputs: %d", static_cast<int>(node->outPins.size()));
    ImGui::Text("Fields: %d", static_cast<int>(node->fields.size()));
    ImGui::Text("Links: %d", CountConnectedLinks(*node));

    if (!node->fields.empty())
    {
        ImGui::Separator();
        ImGui::TextUnformatted("Field values");

        for (const NodeField& field : node->fields)
        {
            ImGui::BulletText("%s (%s): %s",
                field.name.c_str(),
                PinTypeToString(field.valueType),
                field.value.c_str());
        }
    }

    ed::SetCurrentEditor(nullptr);
}

void GraphEditor::DrawNodeCanvas()
{
    ed::Begin("MainGraph");


    ed::Suspend();

    // Handle drag-drop spawn
    const ImGuiPayload* pl = ImGui::GetDragDropPayload();
    if (pl && pl->IsDataType("SPAWN_NODE"))
    {
        ImVec2 dropSize = ImGui::GetContentRegionAvail();
        if (dropSize.x < 1) dropSize.x = 1;
        if (dropSize.y < 1) dropSize.y = 1;

        ImGui::InvisibleButton("##editor_drop_area", dropSize);

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SPAWN_NODE"))
            {
                const NodeSpawnPayload* spawnData =
                    static_cast<const NodeSpawnPayload*>(payload->Data);

                ImVec2 canvasPos = ed::ScreenToCanvas(ImGui::GetMousePos());

                NodeType type = NodeTypeFromString(spawnData->title);
                m_state.AddNode(CreateNodeFromType(type, m_state.GetIdGen(), canvasPos));
            }
            ImGui::EndDragDropTarget();
        }
    }

    ed::Resume();

    // Draw all nodes
    bool anyChanged = false;
for (auto& n : m_state.GetNodes())
    if (n.alive) anyChanged |= DrawVisualNode(n);

    if (anyChanged)
        m_state.MarkDirty();

    // Draw all links
    DrawLinks(m_state.GetLinks());

    // Handle creation and deletion
    if (ed::BeginDelete())
    {
        ed::LinkId linkId;
        DeleteLinks(linkId);

        ed::NodeId nodeId;
        DeleteNodes(nodeId);
    }
    ed::EndDelete();

    if (ed::BeginCreate())
        CreateNewLink();
    ed::EndCreate();

    ed::End();
}



void GraphEditor::DrawContextMenus()
{
    // Placeholder for future context menu logic
}

void GraphEditor::CreateNewLink()
{
    ed::PinId startPinId, endPinId;
    if (!ed::QueryNewLink(&startPinId, &endPinId))
        return;

    const Pin* startPin = m_state.FindPin(startPinId);
    const Pin* endPin   = m_state.FindPin(endPinId);

    if (!startPin || !endPin || !Pin::CanConnect(*startPin, *endPin))
    {
        ed::RejectNewItem();
        return;
    }

    if (ed::AcceptNewItem())
    {
        ed::PinId outPinId = !startPin->isInput ? startPinId : endPinId;
        ed::PinId inPinId  = startPin->isInput ? startPinId : endPinId;

        if (WouldCreateCycle(m_state.GetLinks(), outPinId, inPinId))
        {
            ed::RejectNewItem();
            return;
        }

        const Pin* outPin = !startPin->isInput ? startPin : endPin;

        Link lnk;
        lnk.id         = m_state.GetIdGen().NewLink();
        lnk.startPinId = outPinId;
        lnk.endPinId   = inPinId;
        lnk.type       = outPin->type;
        m_state.AddLink(lnk);
    }
}

void GraphEditor::DeleteNodes(ed::NodeId nodeId)
{
    while (ed::QueryDeletedNode(&nodeId))
    {
        if (!ed::AcceptDeletedItem()) continue;
        m_state.DeleteNode(nodeId);
    }
}

void GraphEditor::DeleteLinks(ed::LinkId linkId)
{
    while (ed::QueryDeletedLink(&linkId))
    {
        if (!ed::AcceptDeletedItem()) continue;
        m_state.DeleteLink(linkId);
    }
}

const VisualNode* GraphEditor::GetPrimarySelectedNode() const
{
    const int selectedCount = ed::GetSelectedObjectCount();
    if (selectedCount <= 0)
        return nullptr;

    std::vector<ed::NodeId> selectedNodes(selectedCount);
    const int nodeCount = ed::GetSelectedNodes(selectedNodes.data(), selectedCount);
    if (nodeCount <= 0)
        return nullptr;

    const ed::NodeId selectedId = selectedNodes.front();
    const auto& nodes = m_state.GetNodes();

    auto it = std::find_if(nodes.begin(), nodes.end(),
        [&](const VisualNode& node)
        {
            return node.alive && node.id == selectedId;
        });

    if (it == nodes.end())
        return nullptr;

    return &(*it);
}

int GraphEditor::CountConnectedLinks(const VisualNode& node) const
{
    auto hasPin = [&](ed::PinId pinId)
    {
        for (const Pin& pin : node.inPins)
            if (pin.id == pinId)
                return true;

        for (const Pin& pin : node.outPins)
            if (pin.id == pinId)
                return true;

        return false;
    };

    int count = 0;
    for (const Link& link : m_state.GetLinks())
    {
        if (hasPin(link.startPinId) || hasPin(link.endPinId))
            ++count;
    }

    return count;
}
