#include "graphEditor.h"
#include "graphState.h"
#include "graphSerializer.h"
#include "graphEditorUtils.h"
#include "editor/renderer/nodeRenderer.h"
#include "editor/renderer/linkRenderer.h"
#include "core/registry/nodeFactory.h"
#include "editor/spawnPayload.h"
#include "editor/panels/inspectorPanel.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

GraphEditor::GraphEditor(ed::EditorContext* ctx, GraphState& state)
    : m_ctx(ctx), m_state(state)
{
}

GraphEditor::~GraphEditor()
{
}

void GraphEditor::Draw()
{
    // The node editor API uses a global current context per frame.
    ed::SetCurrentEditor(m_ctx);

    DrawNodeCanvas();

    ed::SetCurrentEditor(nullptr);
}

void GraphEditor::DrawNodeCanvas()
{
    // Main canvas pass: spawn handling, node/link draw, then post-pass normalization.
    ed::Begin("MainGraph");

    ed::Suspend();

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

                ImVec2 canvasPos = GraphEditorUtils::SnapToNodeGrid(ed::ScreenToCanvas(ImGui::GetMousePos()));

                NodeType type = NodeType::Unknown;
                std::string variableVariant;
                GraphEditorUtils::ParseSpawnPayloadTitle(spawnData->title, type, variableVariant);

                if (type != NodeType::Unknown)
                {
                    VisualNode newNode = CreateNodeFromType(type, m_state.GetIdGen(), canvasPos);

                    if (type == NodeType::Variable && !variableVariant.empty())
                    {
                        if (NodeField* variant = GraphEditorUtils::FindField(newNode.fields, "Variant"))
                            variant->value = (variableVariant == "Get") ? "Get" : "Set";
                    }

                    m_state.AddNode(newNode);
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    ed::Resume();

    if (!m_initialAutoStartHandled)
    {
        if (!m_state.HasAliveNodes())
        {
            // New empty graphs get a Start node so compile/run has a clear entrypoint.
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImVec2 windowSize = ImGui::GetWindowSize();
            const ImVec2 targetScreenPos(
                windowPos.x + windowSize.x * 0.38f,
                windowPos.y + windowSize.y * 0.50f
            );

            const ImVec2 targetCanvasPos = GraphEditorUtils::SnapToNodeGrid(ed::ScreenToCanvas(targetScreenPos));
            m_state.AddNode(CreateNodeFromType(NodeType::Start, m_state.GetIdGen(), targetCanvasPos));
        }

        m_initialAutoStartHandled = true;
    }

    bool anyChanged = false;
    for (auto& n : m_state.GetNodes())
    {
        if (!n.alive)
            continue;

        std::unordered_map<uintptr_t, PinType> pinTypesBefore;
        pinTypesBefore.reserve(n.inPins.size() + n.outPins.size());
        for (const Pin& p : n.inPins)
            pinTypesBefore.emplace(p.id.Get(), p.type);
        for (const Pin& p : n.outPins)
            pinTypesBefore.emplace(p.id.Get(), p.type);

        anyChanged |= DrawVisualNode(n, &m_state.GetIdGen(), &m_state.GetNodes(), &m_state.GetLinks());

        std::vector<ed::PinId> changedPins;
        for (const Pin& p : n.inPins)
        {
            auto it = pinTypesBefore.find(p.id.Get());
            if (it != pinTypesBefore.end() && it->second != p.type)
                changedPins.push_back(p.id);
        }
        for (const Pin& p : n.outPins)
        {
            auto it = pinTypesBefore.find(p.id.Get());
            if (it != pinTypesBefore.end() && it->second != p.type)
                changedPins.push_back(p.id);
        }

        GraphEditorUtils::DisconnectNonAnyLinksForPins(m_state, changedPins);
    }

    if (anyChanged) 
    {
        m_state.MarkDirty();
    }

    DrawLinks(m_state.GetLinks());

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

    // Clicking the same single selected node again toggles selection off.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const ed::NodeId hoveredNode = ed::GetHoveredNode();
        if (hoveredNode.Get() != 0 && ed::IsNodeSelected(hoveredNode))
        {
            const int selectedCount = ed::GetSelectedObjectCount();
            if (selectedCount > 0)
            {
                std::vector<ed::NodeId> selectedNodes(static_cast<size_t>(selectedCount));
                const int selectedNodeCount = ed::GetSelectedNodes(selectedNodes.data(), selectedCount);
                if (selectedNodeCount == 1 && selectedNodes.front() == hoveredNode)
                {
                    ed::DeselectNode(hoveredNode);
                }
            }
        }
    }

    RefreshGraphAndMarkDirty();

    ed::End();
}

void GraphEditor::RefreshGraphAndMarkDirty()
{
    // Keep order stable:
    // descriptor sync can add/remove pins, layout refresh can rename/retype pins,
    // and only then should we prune invalid links.
    const bool descriptorLayoutChanged = GraphEditorUtils::RefreshNodesFromRegistryDescriptors(m_state);
    const bool variableTypesChanged = GraphEditorUtils::RefreshVariableNodeTypes(m_state);
    const bool forEachLayoutChanged = GraphEditorUtils::RefreshForEachNodeLayout(m_state);
    const bool structLayoutChanged = structLikelyDirty
        ? GraphEditorUtils::RefreshStructNodeLayouts(m_state)
        : false;
    const bool callFunctionLayoutChanged = GraphEditorUtils::RefreshCallFunctionNodeLayout(m_state);
    const bool outputInputTypesChanged = GraphEditorUtils::RefreshOutputNodeInputTypes(m_state);
    const bool linksChanged = GraphEditorUtils::SyncLinkTypesAndPruneInvalid(m_state);
    if (descriptorLayoutChanged || variableTypesChanged || forEachLayoutChanged || structLayoutChanged
        || outputInputTypesChanged || linksChanged || callFunctionLayoutChanged)
        m_state.MarkDirty();
}

void GraphEditor::DrawInspectorPanel()
{
    uintptr_t selectedNodeRawId = 0;
    if (!TryGetSingleSelectedNodeId(selectedNodeRawId))
        return;

    InspectorPanel inspector;
    inspector.draw(m_state, selectedNodeRawId);
}

bool GraphEditor::HasSelectedNode() const
{
    uintptr_t selectedNodeId = 0;
    return TryGetSingleSelectedNodeId(selectedNodeId);
}

void GraphEditor::HandleDeleteShortcutFallback()
{
    const int deleteKeyIndex = ImGui::GetKeyIndex(ImGuiKey_Delete);
    const int backspaceKeyIndex = ImGui::GetKeyIndex(ImGuiKey_Backspace);

    const bool deletePressed =
        (deleteKeyIndex >= 0 && ImGui::IsKeyPressed(deleteKeyIndex, false)) ||
        (backspaceKeyIndex >= 0 && ImGui::IsKeyPressed(backspaceKeyIndex, false));

    // Do not steal Delete/Backspace while typing in a text field.
    if (!deletePressed || ImGui::GetIO().WantTextInput)
        return;

    bool anyDeleted = false;

    const int selectedCount = ed::GetSelectedObjectCount();
    if (selectedCount > 0)
    {
        std::vector<ed::LinkId> selectedLinks(static_cast<size_t>(selectedCount));
        const int selectedLinkCount = ed::GetSelectedLinks(selectedLinks.data(), selectedCount);
        for (int i = 0; i < selectedLinkCount; ++i)
        {
            m_state.DeleteLink(selectedLinks[static_cast<size_t>(i)]);
            anyDeleted = true;
        }

        std::vector<ed::NodeId> selectedNodes(static_cast<size_t>(selectedCount));
        const int selectedNodeCount = ed::GetSelectedNodes(selectedNodes.data(), selectedCount);
        for (int i = 0; i < selectedNodeCount; ++i)
        {
            m_state.DeleteNode(selectedNodes[static_cast<size_t>(i)]);
            anyDeleted = true;
        }
    }

    if (anyDeleted)
    {
        GraphEditorUtils::RunAllLayoutRefreshes(m_state);
        GraphEditorUtils::SyncLinkTypesAndPruneInvalid(m_state);
        m_state.MarkDirty();
    }
}

bool GraphEditor::TryGetSingleSelectedNodeId(uintptr_t& outId) const
{
    outId = 0;

    const int selectedCount = ed::GetSelectedObjectCount();
    if (selectedCount <= 0)
        return false;

    std::vector<ed::NodeId> selectedNodes(static_cast<size_t>(selectedCount));
    const int selectedNodeCount = ed::GetSelectedNodes(selectedNodes.data(), selectedCount);
    // Inspector is single-node only to avoid ambiguous multi-edit behavior.
    if (selectedNodeCount != 1)
        return false;

    const ed::NodeId selectedNodeId = selectedNodes.front();
    for (const auto& node : m_state.GetNodes())
    {
        if (node.alive && node.id == selectedNodeId)
        {
            outId = static_cast<uintptr_t>(selectedNodeId.Get());
            return true;
        }
    }

    return false;
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

    // Safety rule:
    // For Each "Array" input must receive Array-compatible data.
    // Allow:
    // - Array (explicit)
    // - Any only from For Each "Element" output (nested array iteration wiring)
    // Reject all other Any/non-array sources to avoid runtime crashes.
    const Pin* outPinPreview = !startPin->isInput ? startPin : endPin;
    const Pin* inPinPreview  = startPin->isInput ? startPin : endPin;
    const bool isNestedForEachElementSource =
        outPinPreview
        && outPinPreview->type == PinType::Any
        && outPinPreview->parentNodeType == NodeType::ForEach
        && outPinPreview->name == "Element";

    if (inPinPreview
        && inPinPreview->parentNodeType == NodeType::ForEach
        && inPinPreview->name == "Array"
        && outPinPreview
        && outPinPreview->type != PinType::Array
        && !isNestedForEachElementSource)
    {
        ed::RejectNewItem();
        return;
    }

    if (ed::AcceptNewItem())
    {
        ed::PinId outPinId = !startPin->isInput ? startPinId : endPinId;
        ed::PinId inPinId  = startPin->isInput ? startPinId : endPinId;
        const Pin* outPin = !startPin->isInput ? startPin : endPin;
        const Pin* inPin  = startPin->isInput ? startPin : endPin;

        std::vector<Link> filteredLinks;
        filteredLinks.reserve(m_state.GetLinks().size());

        const bool isFlowLink = (outPin->type == PinType::Flow);

        // Connection policy:
        // - Flow (white): one outgoing per output pin, allow stacking into same input
        //   (including Debug Print flow input).
        // - Non-flow (typed): allow fan-out from output pin, but keep each input pin single-source.
        const bool replaceExistingFromOutput = isFlowLink;
        const bool replaceExistingToInput = !isFlowLink;

        for (const Link& l : m_state.GetLinks())
        {
            if (!l.alive)
                continue;

            if (replaceExistingFromOutput && l.startPinId == outPinId)
                continue;

            if (replaceExistingToInput && l.endPinId == inPinId)
                continue;

            filteredLinks.push_back(l);
        }

        if (WouldCreateCycle(filteredLinks, outPinId, inPinId))
        {
            ed::RejectNewItem();
            return;
        }

        auto& links = m_state.GetLinks();
        links.erase(
            std::remove_if(links.begin(), links.end(), [&](const Link& l)
            {
                if (!l.alive)
                    return true;

                if (replaceExistingFromOutput && l.startPinId == outPinId)
                    return true;

                if (replaceExistingToInput && l.endPinId == inPinId)
                    return true;

                return false;
            }),
            links.end()
        );

        Link lnk;
        lnk.id         = m_state.GetIdGen().NewLink();
        lnk.startPinId = outPinId;
        lnk.endPinId   = inPinId;
        lnk.type       = outPin->type;
        m_state.AddLink(lnk);

        RefreshGraphAndMarkDirty();
    }
}

void GraphEditor::DeleteNodes(ed::NodeId nodeId)
{
    bool anyDeleted = false;
    while (ed::QueryDeletedNode(&nodeId))
    {
        if (!ed::AcceptDeletedItem()) continue;
        m_state.DeleteNode(nodeId);
        anyDeleted = true;
    }

    if (anyDeleted)
    {
        RefreshGraphAndMarkDirty();
    }
}

void GraphEditor::DeleteLinks(ed::LinkId linkId)
{
    bool anyDeleted = false;
    while (ed::QueryDeletedLink(&linkId))
    {
        if (!ed::AcceptDeletedItem()) continue;
        m_state.DeleteLink(linkId);
        anyDeleted = true;
    }

    if (anyDeleted)
    {
        RefreshGraphAndMarkDirty();
    }
}