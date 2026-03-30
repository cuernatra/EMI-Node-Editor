#include "graphEditor.h"
#include "graphState.h"
#include "graphSerializer.h"
#include "graphEditorUtils.h"
#include "../core/graph/visualNode.h"
#include "../core/graph/link.h"
#include "../core/registry/nodeFactory.h"
#include "imgui.h"
#include <algorithm>
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
    ed::SetCurrentEditor(m_ctx);

    DrawNodeCanvas();

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

    // Draw all nodes
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

    // Open spawn popup at cursor when right-clicking empty node-space background.
    ed::Suspend();
    const bool canvasHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
    const bool rightClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    const bool overNode = ed::GetHoveredNode().Get() != 0;
    const bool overLink = ed::GetHoveredLink().Get() != 0;
    const bool overPin = ed::GetHoveredPin().Get() != 0;
    if (canvasHovered && rightClicked && !overNode && !overLink && !overPin)
    {
        m_spawnPopupScreenPos = ImGui::GetMousePos();
        m_spawnPopupCanvasPos = GraphEditorUtils::SnapToNodeGrid(ed::ScreenToCanvas(m_spawnPopupScreenPos));
        m_openSpawnPopupRequested = true;
    }
    ed::Resume();

    DrawContextMenus();

    // UX rule:
    // If user clicks the same already-selected single node again,
    // deselect it (so inspector closes instead of becoming dim/stacked).
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

    const bool variableTypesChanged = GraphEditorUtils::RefreshVariableNodeTypes(m_state);
    const bool loopLayoutChanged = GraphEditorUtils::RefreshLoopNodeLayout(m_state);
    const bool drawRectLayoutChanged = GraphEditorUtils::RefreshDrawRectNodeLayout(m_state);
    const bool outputInputTypesChanged = GraphEditorUtils::RefreshOutputNodeInputTypes(m_state);
    const bool linksChanged = GraphEditorUtils::SyncLinkTypesAndPruneInvalid(m_state);
    if (variableTypesChanged || loopLayoutChanged || drawRectLayoutChanged || outputInputTypesChanged || linksChanged)
        m_state.MarkDirty();

    ed::End();
}



bool GraphEditor::DrawSpawnNodeMenuContents(const ImVec2& spawnCanvasPos)
{
    auto spawnFromPayloadTitle = [&](const char* payloadTitle) -> bool
    {
        NodeType type = NodeType::Unknown;
        std::string variableVariant;
        GraphEditorUtils::ParseSpawnPayloadTitle(payloadTitle, type, variableVariant);
        if (type == NodeType::Unknown)
            return false;

        if (type == NodeType::Start && m_state.HasNodeType(NodeType::Start))
            return false;

        VisualNode newNode = CreateNodeFromType(type, m_state.GetIdGen(), spawnCanvasPos);

        if (type == NodeType::Variable && !variableVariant.empty())
        {
            if (NodeField* variant = GraphEditorUtils::FindField(newNode.fields, "Variant"))
                variant->value = (variableVariant == "Get") ? "Get" : "Set";
        }

        m_state.AddNode(newNode);
        m_state.MarkDirty();
        return true;
    };

    const bool hasStartNode = m_state.HasNodeType(NodeType::Start);

    if (ImGui::BeginMenu("Events"))
    {
        if (ImGui::MenuItem("Start", nullptr, false, !hasStartNode))
            return spawnFromPayloadTitle("Start");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Data"))
    {
        if (ImGui::MenuItem("Constant"))
            return spawnFromPayloadTitle("Constant");
        if (ImGui::MenuItem("Set Variable"))
            return spawnFromPayloadTitle("Variable:Set");
        if (ImGui::MenuItem("Get Variable"))
            return spawnFromPayloadTitle("Variable:Get");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Logic"))
    {
        if (ImGui::MenuItem("Operator"))
            return spawnFromPayloadTitle("Operator");
        if (ImGui::MenuItem("Comparison"))
            return spawnFromPayloadTitle("Comparison");
        if (ImGui::MenuItem("Logic"))
            return spawnFromPayloadTitle("Logic");
        if (ImGui::MenuItem("Not"))
            return spawnFromPayloadTitle("Not");
        if (ImGui::MenuItem("Draw Grid"))
            return spawnFromPayloadTitle("Draw Grid");
        if (ImGui::MenuItem("Function"))
            return spawnFromPayloadTitle("Function");
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Flow"))
    {
        if (ImGui::MenuItem("Delay"))
            return spawnFromPayloadTitle("Delay");
        if (ImGui::MenuItem("Draw Rect"))
            return spawnFromPayloadTitle("Draw Rect");
        if (ImGui::MenuItem("Sequence"))
            return spawnFromPayloadTitle("Sequence");
        if (ImGui::MenuItem("Branch"))
            return spawnFromPayloadTitle("Branch");
        if (ImGui::MenuItem("Loop"))
            return spawnFromPayloadTitle("Loop");
        if (ImGui::MenuItem("For Each"))
            return spawnFromPayloadTitle("For Each");
        if (ImGui::MenuItem("While"))
            return spawnFromPayloadTitle("While");
        if (ImGui::MenuItem("Debug Print"))
            return spawnFromPayloadTitle("Output");
        ImGui::EndMenu();
    }

    return false;
}


void GraphEditor::DrawContextMenus()
{
    ed::Suspend();

    if (m_openSpawnPopupRequested)
    {
        ImGui::SetNextWindowPos(m_spawnPopupScreenPos, ImGuiCond_Appearing);
        ImGui::OpenPopup("##node_space_spawn_popup");
        m_openSpawnPopupRequested = false;
    }

    if (ImGui::BeginPopup("##node_space_spawn_popup"))
    {
        ImGui::TextUnformatted("Create Node");
        ImGui::Separator();

        if (DrawSpawnNodeMenuContents(m_spawnPopupCanvasPos))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    ed::Resume();
}

void GraphEditor::DrawInspectorPanel()
{
    ImGui::Text("INSPECTOR PANEL");
    ImGui::Separator();

    const int selectedCount = ed::GetSelectedObjectCount();
    if (selectedCount <= 0)
    {
        return;
    }

    std::vector<ed::NodeId> selectedNodes(static_cast<size_t>(selectedCount));
    const int selectedNodeCount = ed::GetSelectedNodes(selectedNodes.data(), selectedCount);
    if (selectedNodeCount <= 0)
    {
        return;
    }

    const ed::NodeId selectedNodeId = selectedNodes.front();

    VisualNode* selectedNode = nullptr;
    for (auto& node : m_state.GetNodes())
    {
        if (!node.alive)
            continue;

        if (node.id == selectedNodeId)
        {
            selectedNode = &node;
            break;
        }
    }

    if (!selectedNode)
    {
        return;
    }

    const ImVec2 nodePos = ed::GetNodePosition(selectedNode->id);

    ImGui::Text("Node ID: %llu", static_cast<unsigned long long>(selectedNode->id.Get()));
    ImGui::Text("Type: %s", NodeTypeToString(selectedNode->nodeType));
    ImGui::Text("Position: (%.1f, %.1f)", nodePos.x, nodePos.y);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("VALUES");
    ImGui::Spacing();

    bool fieldsChanged = false;

    auto isInputPinConnectedByName = [&](const char* pinName) -> bool
    {
        if (!selectedNode || !pinName)
            return false;

        Pin* targetPin = GraphEditorUtils::FindPinByName(selectedNode->inPins, pinName);
        if (!targetPin || targetPin->type == PinType::Flow)
            return false;

        for (const Link& l : m_state.GetLinks())
        {
            if (l.alive && l.endPinId == targetPin->id)
                return true;
        }
        return false;
    };
    if (selectedNode->fields.empty())
    {
        ImGui::TextUnformatted("No values.");
    }
    else
    {
        // Special inspector behavior for Get Variable:
        // - allow selecting only an existing Set variable by name
        // - keep type/default read-only
        // - no other editable fields
        if (selectedNode->nodeType == NodeType::Variable)
        {
            NodeField* variantField = GraphEditorUtils::FindField(selectedNode->fields, "Variant");
            NodeField* nameField = GraphEditorUtils::FindField(selectedNode->fields, "Name");
            NodeField* typeField = GraphEditorUtils::FindField(selectedNode->fields, "Type");

            const bool isGet = variantField && variantField->value == "Get";
            bool defaultConnected = false;
            if (!isGet)
            {
                Pin* defaultPin = GraphEditorUtils::FindPinByName(selectedNode->inPins, "Default");
                if (!defaultPin)
                    defaultPin = GraphEditorUtils::FindPinByName(selectedNode->inPins, "Set");

                if (defaultPin)
                {
                    for (const Link& l : m_state.GetLinks())
                    {
                        if (l.alive && l.endPinId == defaultPin->id)
                        {
                            defaultConnected = true;
                            break;
                        }
                    }
                }
            }

            ImGui::PushID(static_cast<int>(selectedNode->id.Get()));

            if (isGet)
            {
                // Get node inspector:
                // - Variable drop-down from existing Set variables
                std::vector<std::string> setVariableNames;

                for (const auto& node : m_state.GetNodes())
                {
                    if (!node.alive || node.nodeType != NodeType::Variable)
                        continue;

                    const NodeField* nVariant = GraphEditorUtils::FindField(node.fields, "Variant");
                    if (!nVariant || nVariant->value != "Set")
                        continue;

                    const NodeField* nName = GraphEditorUtils::FindField(node.fields, "Name");
                    const std::string varName = nName ? nName->value : "myVar";

                    if (std::find(setVariableNames.begin(), setVariableNames.end(), varName) == setVariableNames.end())
                        setVariableNames.push_back(varName);
                }

                if (nameField)
                {
                    if (setVariableNames.empty())
                    {
                        if (!nameField->value.empty())
                        {
                            nameField->value.clear();
                            fieldsChanged = true;
                        }
                        ImGui::TextUnformatted("Variable");
                        ImGui::SameLine();
                        ImGui::TextDisabled("(no Set variables)");
                    }
                    else
                    {
                        if (std::find(setVariableNames.begin(), setVariableNames.end(), nameField->value) == setVariableNames.end())
                        {
                            nameField->value = setVariableNames.front();
                            fieldsChanged = true;
                        }

                        if (ImGui::BeginCombo("Variable", nameField->value.c_str()))
                        {
                            for (const auto& varName : setVariableNames)
                            {
                                const bool selected = (nameField->value == varName);
                                if (ImGui::Selectable(varName.c_str(), selected))
                                {
                                    nameField->value = varName;
                                    fieldsChanged = true;
                                }
                                if (selected)
                                    ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    }
                }

            }
            else
            {
                for (NodeField& field : selectedNode->fields)
                {
                    if (&field == variantField)
                        continue;

                    if ((field.name == "Default" && defaultConnected)
                        || isInputPinConnectedByName(field.name.c_str()))
                    {
                        GraphEditorUtils::DrawInspectorReadOnlyField(field);
                        continue;
                    }

                    fieldsChanged |= GraphEditorUtils::DrawInspectorField(field);
                }
            }

            ImGui::PopID();
        }
        else if (selectedNode->nodeType == NodeType::ForEach)
        {
            ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
            for (NodeField& field : selectedNode->fields)
            {
                if (isInputPinConnectedByName(field.name.c_str()))
                {
                    GraphEditorUtils::DrawInspectorReadOnlyField(field);
                    continue;
                }

                fieldsChanged |= GraphEditorUtils::DrawInspectorField(field);
            }
            ImGui::PopID();
        }
        else if (selectedNode->nodeType == NodeType::Loop)
        {
            auto isInputPinConnected = [&](const char* pinName) -> bool
            {
                Pin* targetPin = GraphEditorUtils::FindPinByName(selectedNode->inPins, pinName);
                if (!targetPin)
                    return false;

                for (const Link& l : m_state.GetLinks())
                {
                    if (l.alive && l.endPinId == targetPin->id)
                        return true;
                }
                return false;
            };

            ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
            for (NodeField& field : selectedNode->fields)
            {
                if ((field.name == "Start" || field.name == "Count")
                    && isInputPinConnected(field.name.c_str()))
                {
                    GraphEditorUtils::DrawInspectorReadOnlyField(field);
                    continue;
                }

                fieldsChanged |= GraphEditorUtils::DrawInspectorField(field);
            }
            ImGui::PopID();
        }
        else if (selectedNode->nodeType == NodeType::Operator
              || selectedNode->nodeType == NodeType::Comparison
              || selectedNode->nodeType == NodeType::Logic)
        {
            auto isInputPinConnected = [&](const char* pinName) -> bool
            {
                Pin* targetPin = GraphEditorUtils::FindPinByName(selectedNode->inPins, pinName);
                if (!targetPin)
                    return false;

                for (const Link& l : m_state.GetLinks())
                {
                    if (l.alive && l.endPinId == targetPin->id)
                        return true;
                }
                return false;
            };

            ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
            for (NodeField& field : selectedNode->fields)
            {
                if ((field.name == "A" || field.name == "B")
                    && isInputPinConnected(field.name.c_str()))
                {
                    GraphEditorUtils::DrawInspectorReadOnlyField(field);
                    continue;
                }

                fieldsChanged |= GraphEditorUtils::DrawInspectorField(field);
            }
            ImGui::PopID();
        }
        else if (selectedNode->nodeType == NodeType::Not)
        {
            auto isInputPinConnected = [&](const char* pinName) -> bool
            {
                Pin* targetPin = GraphEditorUtils::FindPinByName(selectedNode->inPins, pinName);
                if (!targetPin)
                    return false;

                for (const Link& l : m_state.GetLinks())
                {
                    if (l.alive && l.endPinId == targetPin->id)
                        return true;
                }
                return false;
            };

            ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
            for (NodeField& field : selectedNode->fields)
            {
                if (field.name == "A" && isInputPinConnected("A"))
                {
                    GraphEditorUtils::DrawInspectorReadOnlyField(field);
                    continue;
                }

                fieldsChanged |= GraphEditorUtils::DrawInspectorField(field);
            }
            ImGui::PopID();
        }
        else if (selectedNode->nodeType == NodeType::While)
        {
            ImGui::TextUnformatted("While runs body while Condition is true.");
        }
        else if (selectedNode->nodeType == NodeType::Delay)
        {
            auto isInputPinConnected = [&](const char* pinName) -> bool
            {
                Pin* targetPin = GraphEditorUtils::FindPinByName(selectedNode->inPins, pinName);
                if (!targetPin)
                    return false;

                for (const Link& l : m_state.GetLinks())
                {
                    if (l.alive && l.endPinId == targetPin->id)
                        return true;
                }
                return false;
            };

            ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
            for (NodeField& field : selectedNode->fields)
            {
                if (field.name == "Duration" && isInputPinConnected("Duration"))
                {
                    GraphEditorUtils::DrawInspectorReadOnlyField(field);
                    continue;
                }

                fieldsChanged |= GraphEditorUtils::DrawInspectorField(field);
            }
            ImGui::PopID();
        }
        else
        {
            ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
            for (NodeField& field : selectedNode->fields)
                fieldsChanged |= GraphEditorUtils::DrawInspectorField(field);
            ImGui::PopID();
        }
    }

    if (fieldsChanged)
    {
        GraphSerializer::ApplyConstantTypeFromFields(*selectedNode, /*resetValueOnTypeChange=*/true);

        GraphEditorUtils::RefreshVariableNodeTypes(m_state);
        GraphEditorUtils::RefreshForEachNodeLayout(m_state);
        GraphEditorUtils::RefreshLoopNodeLayout(m_state);
        GraphEditorUtils::RefreshDrawRectNodeLayout(m_state);
        GraphEditorUtils::SyncLinkTypesAndPruneInvalid(m_state);
        m_state.MarkDirty();
    }
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

    // Block only when user is actively editing text.
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
        GraphEditorUtils::RefreshVariableNodeTypes(m_state);
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
    // Inspector is shown only when exactly one node is selected.
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

        const bool variableTypesChanged = GraphEditorUtils::RefreshVariableNodeTypes(m_state);
        const bool loopLayoutChanged = GraphEditorUtils::RefreshLoopNodeLayout(m_state);
        const bool drawRectLayoutChanged = GraphEditorUtils::RefreshDrawRectNodeLayout(m_state);
        const bool linksChanged = GraphEditorUtils::SyncLinkTypesAndPruneInvalid(m_state);
        if (variableTypesChanged || loopLayoutChanged || drawRectLayoutChanged || linksChanged)
            m_state.MarkDirty();
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
        const bool variableTypesChanged = GraphEditorUtils::RefreshVariableNodeTypes(m_state);
        const bool loopLayoutChanged = GraphEditorUtils::RefreshLoopNodeLayout(m_state);
        const bool drawRectLayoutChanged = GraphEditorUtils::RefreshDrawRectNodeLayout(m_state);
        const bool linksChanged = GraphEditorUtils::SyncLinkTypesAndPruneInvalid(m_state);
        if (variableTypesChanged || loopLayoutChanged || drawRectLayoutChanged || linksChanged)
            m_state.MarkDirty();
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
        const bool variableTypesChanged = GraphEditorUtils::RefreshVariableNodeTypes(m_state);
        const bool loopLayoutChanged = GraphEditorUtils::RefreshLoopNodeLayout(m_state);
        const bool drawRectLayoutChanged = GraphEditorUtils::RefreshDrawRectNodeLayout(m_state);
        const bool linksChanged = GraphEditorUtils::SyncLinkTypesAndPruneInvalid(m_state);
        if (variableTypesChanged || loopLayoutChanged || drawRectLayoutChanged || linksChanged)
            m_state.MarkDirty();
    }
}
