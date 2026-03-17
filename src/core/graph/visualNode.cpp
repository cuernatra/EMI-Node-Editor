#include "visualNode.h"
#include "../registry/fieldWidget.h"
#include "editor/graphSerializer.h"
#include "imgui.h"
#include "imgui_node_editor.h"
#include <algorithm>

static bool NodePopupComboDynamic(const char* id,
                                  std::string& value,
                                  const std::vector<std::string>& items,
                                  float width = 110.0f)
{
    bool changed = false;

    ImGui::PushID(id);

    bool open = false;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
    if (ImGui::Button(value.c_str(), ImVec2(width, 0.0f)))
        open = true;

    ImVec2 leftMin = ImGui::GetItemRectMin();
    ImVec2 rightMax = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    float arrowX = rightMax.x - ImGui::GetFrameHeight() * 0.55f;
    float arrowY = (leftMin.y + rightMax.y) * 0.5f;
    draw->AddTriangleFilled(
        ImVec2(arrowX - 4.0f, arrowY - 2.0f),
        ImVec2(arrowX + 4.0f, arrowY - 2.0f),
        ImVec2(arrowX,        arrowY + 3.0f),
        ImGui::GetColorU32(ImGuiCol_Text)
    );
    ImGui::PopStyleVar();

    if (open)
        ImGui::OpenPopup("##popup");

    ed::Suspend();
    ImGui::SetNextWindowPos({ImGui::GetMousePos().x, ImGui::GetMousePos().y}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(40.0f, 0.0f), ImVec2(10000.0f, 10000.0f));
    if (ImGui::BeginPopup("##popup",
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings))
    {
        for (const std::string& item : items)
        {
            const bool selected = (value == item);
            if (ImGui::Selectable(item.c_str(), selected))
            {
                value = item;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndPopup();
    }
    ed::Resume();

    ImGui::PopID();
    return changed;
}

static void DrawReadOnlyField(const NodeField& field)
{
    ImGui::TextUnformatted(field.name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", field.value.c_str());
}

static void DrawPin(const Pin& pin)
{

    ImVec4 col = pin.GetTypeColor();
    ImGui::PushStyleColor(ImGuiCol_Text, col);

    if (pin.isInput)
    {
        ed::BeginPin(pin.id, ed::PinKind::Input);
        ImGui::Text("-> %s", pin.name.c_str());
    }
    else
    {   
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 100);
        ed::BeginPin(pin.id, ed::PinKind::Output);
        ImGui::Text("%s ->", pin.name.c_str());
    }
    ImGui::PopStyleColor();

    ed::EndPin();
}

bool DrawVisualNode(VisualNode& n)
{
    return DrawVisualNode(n, nullptr, nullptr, nullptr);
}

bool DrawVisualNode(VisualNode& n, IdGen* idGen)
{
    return DrawVisualNode(n, idGen, nullptr, nullptr);
}

bool DrawVisualNode(VisualNode& n, IdGen* idGen, const std::vector<VisualNode>* allNodes)
{
    return DrawVisualNode(n, idGen, allNodes, nullptr);
}

bool DrawVisualNode(VisualNode& n, IdGen* idGen, const std::vector<VisualNode>* allNodes, const std::vector<Link>* allLinks)
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
        auto findFieldValue = [&](const char* name) -> const std::string*
        {
            for (const NodeField& f : n.fields)
                if (f.name == name)
                    return &f.value;
            return nullptr;
        };

        const std::string* variant = findFieldValue("Variant");
        const bool isGetVariable =
            (n.nodeType == NodeType::Variable && variant && *variant == "Get");
        const bool isSetVariable =
            (n.nodeType == NodeType::Variable && variant && *variant == "Set");

        auto isInputPinConnected = [&](const char* pinName) -> bool
        {
            if (!allLinks)
                return false;

            const Pin* targetPin = nullptr;
            for (const Pin& p : n.inPins)
            {
                if (p.name == pinName)
                {
                    targetPin = &p;
                    break;
                }
            }
            if (!targetPin)
                return false;

            for (const Link& l : *allLinks)
            {
                if (l.alive && l.endPinId == targetPin->id)
                    return true;
            }
            return false;
        };

        ImGui::Spacing();
        ImGui::PushID((int)n.id.Get());
        for (NodeField& field : n.fields)
        {
            if (field.name == "Variant")
                continue; // internal bookkeeping field, hide from node body

            if (isGetVariable && field.name == "Type")
                continue; // Keep Type hidden for Get node body

            if (isGetVariable && field.name == "Default")
                continue; // Get node should not expose editable default in node body

            if (isGetVariable && field.name == "Name")
            {
                std::vector<std::string> setVariableNames;

                if (allNodes)
                {
                    for (const VisualNode& other : *allNodes)
                    {
                        if (!other.alive || other.nodeType != NodeType::Variable)
                            continue;

                        const NodeField* otherVariant = nullptr;
                        const NodeField* otherName = nullptr;
                        for (const NodeField& of : other.fields)
                        {
                            if (of.name == "Variant") otherVariant = &of;
                            if (of.name == "Name") otherName = &of;
                        }

                        if (!otherVariant || otherVariant->value != "Set")
                            continue;

                        const std::string variableName = otherName ? otherName->value : "myVar";
                        if (std::find(setVariableNames.begin(), setVariableNames.end(), variableName) == setVariableNames.end())
                            setVariableNames.push_back(variableName);
                    }
                }

                if (setVariableNames.empty())
                {
                    ImGui::TextUnformatted("Variable");
                    ImGui::SameLine();
                    ImGui::TextDisabled("(none)");
                }
                else
                {
                    if (std::find(setVariableNames.begin(), setVariableNames.end(), field.value) == setVariableNames.end())
                    {
                        field.value = setVariableNames.front();
                        changed = true;
                    }

                    ImGui::TextUnformatted("Variable");
                    ImGui::SameLine();
                    changed |= NodePopupComboDynamic("##GetVariableNodeCombo", field.value, setVariableNames, 110.0f);
                }

                continue;
            }

            if (isSetVariable && field.name == "Default")
            {
                const bool defaultConnected = isInputPinConnected("Default");
                if (defaultConnected)
                {
                    DrawReadOnlyField(field);
                }
                else
                {
                    changed |= DrawField(field);
                }
                continue;
            }

            changed |= DrawField(field);
        }
        ImGui::PopID();
        ImGui::Spacing();
    }

    if (changed)
        GraphSerializer::ApplyConstantTypeFromFields(n, /*resetValueOnTypeChange=*/true);

    if (n.nodeType == NodeType::Sequence && idGen)
    {
        if (ImGui::SmallButton("+ Then"))
        {
            const int thenIndex = static_cast<int>(n.outPins.size());
            n.outPins.push_back(MakePin(
                static_cast<uint32_t>(idGen->NewPin().Get()),
                n.id,
                n.nodeType,
                "Then " + std::to_string(thenIndex),
                PinType::Flow,
                false
            ));
            changed = true;
        }
    }

    for (const Pin& pin : n.outPins)
        DrawPin(pin);

    ed::EndNode();
    return changed;
}

