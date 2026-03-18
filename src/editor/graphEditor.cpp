#include "graphEditor.h"
#include "graphState.h"
#include "graphSerializer.h"
#include "../core/graph/visualNode.h"
#include "../core/graph/link.h"
#include "../core/registry/nodeFactory.h"
#include "imgui.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace
{
float ParseInspectorFloat(const std::string& s)
{
    if (s.empty())
        return 0.0f;

    try
    {
        return std::stof(s);
    }
    catch (...)
    {
        return 0.0f;
    }
}

bool ParseInspectorBool(const std::string& s)
{
    return s == "1" || s == "true" || s == "True";
}

NodeField* FindField(std::vector<NodeField>& fields, const char* name)
{
    for (auto& f : fields)
        if (f.name == name)
            return &f;
    return nullptr;
}

const NodeField* FindField(const std::vector<NodeField>& fields, const char* name)
{
    for (const auto& f : fields)
        if (f.name == name)
            return &f;
    return nullptr;
}

bool DrawInspectorField(NodeField& field)
{
    bool changed = false;
    ImGui::PushID(field.name.c_str());

    switch (field.valueType)
    {
        case PinType::Number:
        {
            float v = ParseInspectorFloat(field.value);
            if (ImGui::InputFloat(field.name.c_str(), &v, 0.0f, 0.0f, "%.3f"))
            {
                field.value = std::to_string(v);
                changed = true;
            }
            break;
        }

        case PinType::Boolean:
        {
            bool v = ParseInspectorBool(field.value);
            if (ImGui::Checkbox(field.name.c_str(), &v))
            {
                field.value = v ? "true" : "false";
                changed = true;
            }
            break;
        }

        case PinType::String:
        {
            if (field.name == "Op")
            {
                const char* items[] = { "+", "-", "*", "/", "==", "!=", "<", "<=", ">", ">=", "AND", "OR", "NOT" };
                if (ImGui::BeginCombo("Op", field.value.c_str()))
                {
                    for (const char* item : items)
                    {
                        const bool isSelected = (field.value == item);
                        if (ImGui::Selectable(item, isSelected))
                        {
                            field.value = item;
                            changed = true;
                        }
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            else if (field.name == "Type")
            {
                const char* items[] = { "Number", "Boolean", "String", "Array" };
                if (ImGui::BeginCombo("Type", field.value.c_str()))
                {
                    for (const char* item : items)
                    {
                        const bool isSelected = (field.value == item);
                        if (ImGui::Selectable(item, isSelected))
                        {
                            field.value = item;
                            changed = true;
                        }
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            else
            {
                char buf[128] = {};
                std::strncpy(buf, field.value.c_str(), sizeof(buf) - 1);
                if (ImGui::InputText(field.name.c_str(), buf, sizeof(buf)))
                {
                    field.value = buf;
                    changed = true;
                }
            }
            break;
        }

        case PinType::Array:
        {
            char buf[128] = {};
            std::strncpy(buf, field.value.c_str(), sizeof(buf) - 1);
            if (ImGui::InputText(field.name.c_str(), buf, sizeof(buf)))
            {
                field.value = buf;
                changed = true;
            }
            break;
        }

        default:
            ImGui::LabelText(field.name.c_str(), "%s", field.value.c_str());
            break;
    }

    ImGui::PopID();
    return changed;
}

bool RefreshVariableNodeTypes(GraphState& state)
{
    bool changedAnyPass = false;

    auto& nodes = state.GetNodes();
    const auto& links = state.GetLinks();

    const size_t maxPasses = nodes.size() + 1;
    for (size_t pass = 0; pass < maxPasses; ++pass)
    {
        bool changedThisPass = false;

        for (auto& n : nodes)
        {
            if (!n.alive || n.nodeType != NodeType::Variable)
                continue;

            Pin* setPin = nullptr;
            Pin* valuePin = nullptr;

            for (Pin& p : n.inPins)
                if (p.name == "Set") { setPin = &p; break; }

            for (Pin& p : n.outPins)
                if (p.name == "Value") { valuePin = &p; break; }

            if (!setPin || !valuePin)
                continue;

            // Variable type is driven ONLY by what is connected into Set.
            // If Set is disconnected (or connected from Any), Variable is Any.
            PinType resolved = PinType::Any;

            for (const Link& l : links)
            {
                if (!l.alive || l.endPinId != setPin->id)
                    continue;

                const Pin* source = state.FindPin(l.startPinId);
                if (source)
                    resolved = source->type;
                else
                    resolved = l.type;

                break;
            }

            if (setPin->type != resolved)
            {
                setPin->type = resolved;
                changedThisPass = true;
            }

            if (valuePin->type != resolved)
            {
                valuePin->type = resolved;
                changedThisPass = true;
            }
        }

        changedAnyPass = changedAnyPass || changedThisPass;
        if (!changedThisPass)
            break;
    }

    return changedAnyPass;
}

bool SyncLinkTypesAndPruneInvalid(GraphState& state)
{
    bool changed = false;
    auto& links = state.GetLinks();

    links.erase(
        std::remove_if(links.begin(), links.end(), [&](const Link& l)
        {
            const Pin* start = state.FindPin(l.startPinId);
            const Pin* end   = state.FindPin(l.endPinId);
            if (!start || !end)
            {
                changed = true;
                return true;
            }

            const Pin* outPin = !start->isInput ? start : end;
            const Pin* inPin  = start->isInput ? start : end;

            if (!Pin::CanConnect(*outPin, *inPin))
            {
                changed = true;
                return true;
            }

            return false;
        }),
        links.end()
    );

    for (Link& l : links)
    {
        const Pin* start = state.FindPin(l.startPinId);
        if (!start)
            continue;

        if (l.type != start->type)
        {
            l.type = start->type;
            changed = true;
        }
    }

    return changed;
}

void DisconnectNonAnyLinksForPins(GraphState& state, const std::vector<ed::PinId>& pinIds)
{
    if (pinIds.empty())
        return;

    auto touchesChangedPin = [&](const Link& l)
    {
        for (ed::PinId id : pinIds)
        {
            if (l.startPinId == id || l.endPinId == id)
                return true;
        }
        return false;
    };

    auto& links = state.GetLinks();
    const size_t before = links.size();

    links.erase(
        std::remove_if(links.begin(), links.end(), [&](const Link& l)
        {
            if (!touchesChangedPin(l))
                return false;

            // Keep white (Any) links, remove typed links when pin type changes.
            return l.type != PinType::Any;
        }),
        links.end()
    );

    if (links.size() != before)
        state.MarkDirty();
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
    {
        if (!n.alive)
            continue;

        std::unordered_map<uintptr_t, PinType> pinTypesBefore;
        pinTypesBefore.reserve(n.inPins.size() + n.outPins.size());
        for (const Pin& p : n.inPins)
            pinTypesBefore.emplace(p.id.Get(), p.type);
        for (const Pin& p : n.outPins)
            pinTypesBefore.emplace(p.id.Get(), p.type);

        anyChanged |= DrawVisualNode(n, &m_state.GetIdGen());

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

        DisconnectNonAnyLinksForPins(m_state, changedPins);
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

    const bool variableTypesChanged = RefreshVariableNodeTypes(m_state);
    const bool linksChanged = SyncLinkTypesAndPruneInvalid(m_state);
    if (variableTypesChanged || linksChanged)
        m_state.MarkDirty();

    ed::End();
}



void GraphEditor::DrawContextMenus()
{
    // Placeholder for future context menu logic
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
            NodeField* variantField = FindField(selectedNode->fields, "Variant");
            NodeField* nameField = FindField(selectedNode->fields, "Name");
            NodeField* typeField = FindField(selectedNode->fields, "Type");
            NodeField* defaultField = FindField(selectedNode->fields, "Default");

            const bool isGet = variantField && variantField->value == "Get";

            ImGui::PushID(static_cast<int>(selectedNode->id.Get()));

            if (isGet)
            {
                // Get node inspector:
                // - Variable drop-down from existing Set variables
                // - Default remains editable fallback value
                std::vector<std::string> setVariableNames;

                for (const auto& node : m_state.GetNodes())
                {
                    if (!node.alive || node.nodeType != NodeType::Variable)
                        continue;

                    const NodeField* nVariant = FindField(node.fields, "Variant");
                    if (!nVariant || nVariant->value != "Set")
                        continue;

                    const NodeField* nName = FindField(node.fields, "Name");
                    const std::string varName = nName ? nName->value : "myVar";

                    if (std::find(setVariableNames.begin(), setVariableNames.end(), varName) == setVariableNames.end())
                        setVariableNames.push_back(varName);
                }

                if (nameField)
                {
                    if (setVariableNames.empty())
                    {
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

                if (defaultField)
                    fieldsChanged |= DrawInspectorField(*defaultField);
            }
            else
            {
                for (NodeField& field : selectedNode->fields)
                {
                    if (&field == variantField)
                        continue;
                    fieldsChanged |= DrawInspectorField(field);
                }
            }

            ImGui::PopID();
        }
        else if (selectedNode->nodeType == NodeType::FunctionCall)
        {
            // FunctionCall inspector: editable fields + +/- buttons for argument pins.
            ImGui::PushID(static_cast<int>(selectedNode->id.Get()));

            for (NodeField& field : selectedNode->fields)
            {
                if (field.name == "ArgCount") continue;   // managed by buttons below
                fieldsChanged |= DrawInspectorField(field);
            }

            // Compute current arg count from Arg pins.
            int currentArgCount = 0;
            for (const Pin& p : selectedNode->inPins)
                if (p.name.rfind("Arg", 0) == 0) ++currentArgCount;

            ImGui::Spacing();
            ImGui::Text("Arguments: %d", currentArgCount);
            ImGui::SameLine();

            if (ImGui::Button("+"))
            {
                const int newIdx = currentArgCount;
                selectedNode->inPins.push_back(MakePin(
                    static_cast<uint32_t>(m_state.GetIdGen().NewPin().Get()),
                    selectedNode->id, selectedNode->nodeType,
                    "Arg" + std::to_string(newIdx),
                    PinType::Any, true));

                // Update ArgCount field.
                for (NodeField& f : selectedNode->fields)
                    if (f.name == "ArgCount") { f.value = std::to_string(newIdx + 1); break; }

                fieldsChanged = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("-") && currentArgCount > 0)
            {
                // Remove the last Arg pin and its connected link.
                for (int i = static_cast<int>(selectedNode->inPins.size()) - 1; i >= 0; --i)
                {
                    if (selectedNode->inPins[static_cast<size_t>(i)].name.rfind("Arg", 0) == 0)
                    {
                        const ed::PinId removedId = selectedNode->inPins[static_cast<size_t>(i)].id;
                        selectedNode->inPins.erase(selectedNode->inPins.begin() + i);

                        auto& links = m_state.GetLinks();
                        links.erase(std::remove_if(links.begin(), links.end(),
                            [removedId](const Link& l) {
                                return l.startPinId == removedId || l.endPinId == removedId;
                            }), links.end());
                        break;
                    }
                }

                // Update ArgCount field.
                for (NodeField& f : selectedNode->fields)
                    if (f.name == "ArgCount") { f.value = std::to_string(currentArgCount - 1); break; }

                fieldsChanged = true;
            }

            ImGui::PopID();
        }
        else
        {
            ImGui::PushID(static_cast<int>(selectedNode->id.Get()));
            for (NodeField& field : selectedNode->fields)
                fieldsChanged |= DrawInspectorField(field);
            ImGui::PopID();
        }
    }

    if (fieldsChanged)
    {
        GraphSerializer::ApplyConstantTypeFromFields(*selectedNode, /*resetValueOnTypeChange=*/true);

        RefreshVariableNodeTypes(m_state);
        SyncLinkTypesAndPruneInvalid(m_state);
        m_state.MarkDirty();
    }
}

bool GraphEditor::HasSelectedNode() const
{
    const int selectedCount = ed::GetSelectedObjectCount();
    if (selectedCount <= 0)
        return false;

    std::vector<ed::NodeId> selectedNodes(static_cast<size_t>(selectedCount));
    const int selectedNodeCount = ed::GetSelectedNodes(selectedNodes.data(), selectedCount);
    if (selectedNodeCount <= 0)
        return false;

    for (int i = 0; i < selectedNodeCount; ++i)
    {
        const ed::NodeId selectedNodeId = selectedNodes[static_cast<size_t>(i)];
        for (const auto& node : m_state.GetNodes())
        {
            if (node.alive && node.id == selectedNodeId)
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

    if (ed::AcceptNewItem())
    {
        ed::PinId outPinId = !startPin->isInput ? startPinId : endPinId;
        ed::PinId inPinId  = startPin->isInput ? startPinId : endPinId;
        const Pin* outPin = !startPin->isInput ? startPin : endPin;
        const Pin* inPin  = startPin->isInput ? startPin : endPin;

        std::vector<Link> filteredLinks;
        filteredLinks.reserve(m_state.GetLinks().size());

        const bool replaceExistingFromOutput = true;
        const bool isNonFlowInput = (inPin->type != PinType::Flow);
        const bool isOutputNodeInput = (inPin->parentNodeType == NodeType::Output);
        const bool replaceExistingToInput = isOutputNodeInput || isNonFlowInput || !inPin->isMultiInput;

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

        const bool variableTypesChanged = RefreshVariableNodeTypes(m_state);
        const bool linksChanged = SyncLinkTypesAndPruneInvalid(m_state);
        if (variableTypesChanged || linksChanged)
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
        const bool variableTypesChanged = RefreshVariableNodeTypes(m_state);
        const bool linksChanged = SyncLinkTypesAndPruneInvalid(m_state);
        if (variableTypesChanged || linksChanged)
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
        const bool variableTypesChanged = RefreshVariableNodeTypes(m_state);
        const bool linksChanged = SyncLinkTypesAndPruneInvalid(m_state);
        if (variableTypesChanged || linksChanged)
            m_state.MarkDirty();
    }
}
