#include "nodeRenderer.h"

#include "fieldWidgetRenderer.h"
#include "pinRenderer.h"

#include "app/constants.h"
#include "core/registry/nodeRegistry.h"
#include "editor/nodeColorCategories.h"
#include "editor/settings.h"
#include "editor/graph/graphSerializer.h"

#include "imgui.h"
#include "imgui_node_editor.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <unordered_map>
#include <utility>

namespace ed = ax::NodeEditor;

namespace
{
ImVec4 GetNodeCategoryHeaderColor(const NodeDescriptor* desc)
{
    switch (GetNodeColorCategory(desc))
    {
        case NodeColorCategory::Event:
            return ImVec4(Settings::nodeHeaderEventColorR, Settings::nodeHeaderEventColorG, Settings::nodeHeaderEventColorB, Settings::nodeHeaderEventColorA);
        case NodeColorCategory::Data:
            return ImVec4(Settings::nodeHeaderDataColorR, Settings::nodeHeaderDataColorG, Settings::nodeHeaderDataColorB, Settings::nodeHeaderDataColorA);
        case NodeColorCategory::Struct:
            return ImVec4(Settings::nodeHeaderStructColorR, Settings::nodeHeaderStructColorG, Settings::nodeHeaderStructColorB, Settings::nodeHeaderStructColorA);
        case NodeColorCategory::Logic:
            return ImVec4(Settings::nodeHeaderLogicColorR, Settings::nodeHeaderLogicColorG, Settings::nodeHeaderLogicColorB, Settings::nodeHeaderLogicColorA);
        case NodeColorCategory::Flow:
            return ImVec4(Settings::nodeHeaderFlowColorR, Settings::nodeHeaderFlowColorG, Settings::nodeHeaderFlowColorB, Settings::nodeHeaderFlowColorA);
        case NodeColorCategory::More:
        default:
            return ImVec4(Settings::nodeHeaderMoreColorR, Settings::nodeHeaderMoreColorG, Settings::nodeHeaderMoreColorB, Settings::nodeHeaderMoreColorA);
    }
}

NodeField* FindFieldByName(VisualNode& node, const char* name)
{
    for (NodeField& field : node.fields)
    {
        if (field.name == name)
            return &field;
    }
    return nullptr;
}

const NodeField* FindFieldByName(const VisualNode& node, const char* name)
{
    for (const NodeField& field : node.fields)
    {
        if (field.name == name)
            return &field;
    }
    return nullptr;
}

float ParseNodeFloat(const std::string& value, float fallback = 0.0f)
{
    try
    {
        return std::stof(value);
    }
    catch (...)
    {
        return fallback;
    }
}

std::string FormatFloatString(float value)
{
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%.3f", value);
    return buffer;
}

std::string FormatColorString(int r, int g, int b)
{
    return std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b);
}

bool TryParseColorString(const std::string& text, std::array<int, 3>& rgb)
{
    const char* cursor = text.c_str();
    char* end = nullptr;

    for (int i = 0; i < 3; ++i)
    {
        while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)))
            ++cursor;

        const long component = std::strtol(cursor, &end, 10);
        if (end == cursor)
            return false;

        rgb[i] = static_cast<int>(std::clamp(component, 0L, 255L));
        cursor = end;

        while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)))
            ++cursor;

        if (i < 2)
        {
            if (*cursor != ',')
                return false;
            ++cursor;
        }
    }

    while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)))
        ++cursor;

    return *cursor == '\0';
}

bool ClampFieldToRange(NodeField& field, float minValue, float maxValue)
{
    const float clamped = std::clamp(ParseNodeFloat(field.value, minValue), minValue, maxValue);
    const std::string normalized = FormatFloatString(clamped);
    if (field.value != normalized)
    {
        field.value = normalized;
        return true;
    }
    return false;
}

bool SyncDrawNodeColorFields(VisualNode& node, bool preferColorText)
{
    NodeField* colorField = FindFieldByName(node, "Color");
    NodeField* rField = FindFieldByName(node, "R");
    NodeField* gField = FindFieldByName(node, "G");
    NodeField* bField = FindFieldByName(node, "B");
    if (!colorField || !rField || !gField || !bField)
        return false;

    bool changed = false;

    changed |= ClampFieldToRange(*rField, 0.0f, 255.0f);
    changed |= ClampFieldToRange(*gField, 0.0f, 255.0f);
    changed |= ClampFieldToRange(*bField, 0.0f, 255.0f);

    if (preferColorText)
    {
        std::array<int, 3> rgb{};
        if (TryParseColorString(colorField->value, rgb))
        {
            const std::string rText = FormatFloatString(static_cast<float>(rgb[0]));
            const std::string gText = FormatFloatString(static_cast<float>(rgb[1]));
            const std::string bText = FormatFloatString(static_cast<float>(rgb[2]));
            if (rField->value != rText) { rField->value = rText; changed = true; }
            if (gField->value != gText) { gField->value = gText; changed = true; }
            if (bField->value != bText) { bField->value = bText; changed = true; }

            const std::string normalizedColor = FormatColorString(rgb[0], rgb[1], rgb[2]);
            if (colorField->value != normalizedColor)
            {
                colorField->value = normalizedColor;
                changed = true;
            }
            return changed;
        }
    }

    const int r = static_cast<int>(std::clamp(std::lround(ParseNodeFloat(rField->value)), 0L, 255L));
    const int g = static_cast<int>(std::clamp(std::lround(ParseNodeFloat(gField->value)), 0L, 255L));
    const int b = static_cast<int>(std::clamp(std::lround(ParseNodeFloat(bField->value)), 0L, 255L));
    const std::string normalizedColor = FormatColorString(r, g, b);
    if (colorField->value != normalizedColor)
    {
        colorField->value = normalizedColor;
        changed = true;
    }

    return changed;
}

bool ClampDrawNodeGeometryFields(VisualNode& node)
{
    bool changed = false;

    if (NodeField* x = FindFieldByName(node, "X")) changed |= ClampFieldToRange(*x, 0.0f, 100000.0f);
    if (NodeField* y = FindFieldByName(node, "Y")) changed |= ClampFieldToRange(*y, 0.0f, 100000.0f);
    if (NodeField* w = FindFieldByName(node, "W")) changed |= ClampFieldToRange(*w, 0.0f, 100000.0f);
    if (NodeField* h = FindFieldByName(node, "H")) changed |= ClampFieldToRange(*h, 0.0f, 100000.0f);

    return changed;
}

std::string TrimArrayToken(const std::string& s)
{
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::vector<std::string> ParseArrayItemsForNodeView(const std::string& text)
{
    std::string src = TrimArrayToken(text);
    if (src.size() >= 2 && src.front() == '[' && src.back() == ']')
        src = src.substr(1, src.size() - 2);

    std::vector<std::string> out;
    std::string current;
    current.reserve(src.size());

    int bracketDepth = 0;
    int parenDepth = 0;
    int braceDepth = 0;
    bool inQuote = false;
    char quoteChar = '\0';
    bool escape = false;

    auto pushCurrent = [&]()
    {
        std::string item = TrimArrayToken(current);
        if (!item.empty())
            out.push_back(item);
        current.clear();
    };

    for (char ch : src)
    {
        if (escape)
        {
            current.push_back(ch);
            escape = false;
            continue;
        }

        if (inQuote)
        {
            current.push_back(ch);
            if (ch == '\\')
                escape = true;
            else if (ch == quoteChar)
                inQuote = false;
            continue;
        }

        if (ch == '"' || ch == '\'')
        {
            inQuote = true;
            quoteChar = ch;
            current.push_back(ch);
            continue;
        }

        if (ch == '[') ++bracketDepth;
        else if (ch == ']') bracketDepth = std::max(0, bracketDepth - 1);
        else if (ch == '(') ++parenDepth;
        else if (ch == ')') parenDepth = std::max(0, parenDepth - 1);
        else if (ch == '{') ++braceDepth;
        else if (ch == '}') braceDepth = std::max(0, braceDepth - 1);

        if (ch == ',' && bracketDepth == 0 && parenDepth == 0 && braceDepth == 0)
        {
            pushCurrent();
            continue;
        }

        current.push_back(ch);
    }

    pushCurrent();
    return out;
}

static std::unordered_map<ImGuiID, bool> s_nodeArrayOpenState;
static std::unordered_map<std::string, int> s_arrayItemCountCache;
static std::unordered_map<std::string, std::vector<std::string>> s_structFieldNameCache;

int GetArrayItemCountCached(const std::string& text)
{
    auto it = s_arrayItemCountCache.find(text);
    if (it != s_arrayItemCountCache.end())
        return it->second;

    const int count = static_cast<int>(ParseArrayItemsForNodeView(text).size());
    s_arrayItemCountCache[text] = count;
    return count;
}

const std::vector<std::string>& GetStructFieldNamesCached(const std::string& schemaText)
{
    auto it = s_structFieldNameCache.find(schemaText);
    if (it != s_structFieldNameCache.end())
        return it->second;

    std::vector<std::string> names;
    const std::vector<std::string> defs = ParseArrayItemsForNodeView(schemaText);
    for (std::string raw : defs)
    {
        raw = TrimArrayToken(raw);
        if (raw.size() >= 2
            && ((raw.front() == '"' && raw.back() == '"')
                || (raw.front() == '\'' && raw.back() == '\'')))
        {
            raw = raw.substr(1, raw.size() - 2);
        }

        const size_t sep = raw.find(':');
        const std::string name = TrimArrayToken(
            (sep == std::string::npos) ? raw : raw.substr(0, sep)
        );

        if (!name.empty()
            && std::find(names.begin(), names.end(), name) == names.end())
        {
            names.push_back(name);
        }
    }

    auto [insertedIt, _] = s_structFieldNameCache.emplace(schemaText, std::move(names));
    return insertedIt->second;
}
}

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
        ImGui::GetColorU32(colors::textPrimary)
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

bool IsDeferredInputPin(const NodeDescriptor* desc, const Pin& pin)
{
    if (!desc)
        return false;

    // StructCreate schema-field pins are drawn inline with their field widgets in
    // HandleStructStyleField. They are dynamic (not known at descriptor time), so
    // they cannot be listed in deferredInputPins statically. Defer everything
    // except the "Struct" schema-connection pin.
    if (desc->renderStyle == NodeRenderStyle::StructCreate && pin.name != "Struct")
        return true;

    return std::find(desc->deferredInputPins.begin(), desc->deferredInputPins.end(), pin.name) != desc->deferredInputPins.end();
}

float MeasureFieldWidth(NodeRenderStyle renderStyle, const NodeField& field)
{
    const float labelWidth  = ImGui::CalcTextSize(field.name.c_str()).x;
    float widgetWidth = 82.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;

    // Keep Constant node much narrower in graph view.
    if (renderStyle == NodeRenderStyle::Constant)
    {
        widgetWidth = 58.0f;
        spacing = 4.0f;
    }

    return labelWidth + spacing + widgetWidth;
}

float MeasureNodeContentWidth(const VisualNode& n, const NodeDescriptor* desc)
{
    const float iconWidth = 14.0f;
    const float pinGap    = 6.0f;
    const float pinRowPad = iconWidth + pinGap;
    const NodeRenderStyle renderStyle = desc ? desc->renderStyle : NodeRenderStyle::Default;

    float maxWidth = ImGui::CalcTextSize(n.title.c_str()).x;

    for (const Pin& pin : n.inPins)
        maxWidth = std::max(maxWidth, pinRowPad + ImGui::CalcTextSize(pin.name.c_str()).x);

    for (const Pin& pin : n.outPins)
        maxWidth = std::max(maxWidth, ImGui::CalcTextSize(pin.name.c_str()).x + pinGap + iconWidth);

    for (const NodeField& field : n.fields)
    {
        if (field.name == "Variant") continue;
        maxWidth = std::max(maxWidth, MeasureFieldWidth(renderStyle, field));
    }

    return maxWidth;
}

void DrawPin(const Pin& pin, float contentWidth, const std::vector<Link>* allLinks)
{
    const ImVec4      colorF    = GetPinTypeColor(pin.type);
    const ImU32       iconColor = ImGui::ColorConvertFloat4ToU32(colorF);
    const ImU32       innerColor = ImGui::GetColorU32(colors::background);
    const ImVec2      iconSize(14.0f, 14.0f);
    const PinIconType iconType = GetPinTypeIcon(pin.type);

    bool iconFilled = false;
    if (allLinks)
    {
        for (const Link& link : *allLinks)
        {
            const bool connected = link.alive && (pin.isInput
                ? link.endPinId   == pin.id
                : link.startPinId == pin.id);
            if (connected) { iconFilled = true; break; }
        }
    }

    auto drawIcon = [&]() -> std::pair<ImVec2, ImVec2>
    {
        const ImVec2 tl = ImGui::GetCursorScreenPos();
        const ImVec2 br(tl.x + iconSize.x, tl.y + iconSize.y);
        DrawPinIcon(ImGui::GetWindowDrawList(), tl, br, iconType, iconFilled, iconColor, innerColor);
        ImGui::Dummy(iconSize);
        return { tl, br };
    };

    ImGui::PushStyleColor(ImGuiCol_Text, colors::textPrimary);
    ed::BeginPin(pin.id, pin.isInput ? ed::PinKind::Input : ed::PinKind::Output);

    if (pin.isInput)
    {
        auto [tl, br] = drawIcon();
        const ImVec2 leftEdge(tl.x, (tl.y + br.y) * 0.5f);
        ed::PinPivotRect(leftEdge, leftEdge);

        ImGui::SameLine(0.0f, 6.0f);
        ImGui::TextUnformatted(pin.name.c_str());
    }
    else
    {
        const float   gap       = 6.0f;
        const float   textWidth = ImGui::CalcTextSize(pin.name.c_str()).x;
        const ImVec2  rowStart  = ImGui::GetCursorPos();
        const float   iconX     = rowStart.x + contentWidth - iconSize.x;
        const float   textX     = iconX - gap - textWidth;

        ImGui::SetCursorPos(ImVec2(std::max(textX, rowStart.x), rowStart.y));
        ImGui::TextUnformatted(pin.name.c_str());

        ImGui::SetCursorPos(ImVec2(std::max(iconX, rowStart.x), rowStart.y));
        auto [tl, br] = drawIcon();
        const ImVec2 rightEdge(br.x, (tl.y + br.y) * 0.5f);
        ed::PinPivotRect(rightEdge, rightEdge);
    }

    ed::EndPin();
    ImGui::PopStyleColor();
}

// These structs are defined before NodeRendererSpecialCases so FieldRenderContext
// can hold references to them without forward declarations.
struct NodeRenderFlags
{
    bool isGetVariable      = false;
    bool isSetVariable      = false;
    bool isForEachNode      = false;
    bool isLoopNode         = false;
    bool isBinaryDefaultNode = false;
    bool isUnaryDefaultNode = false;
    bool isDelayNode        = false;
    bool isDrawNode         = false;
    bool isStructDefineNode = false;
    bool isStructCreateNode = false;
    bool isArrayIndexNode   = false;
    bool isArrayAddNode     = false;
    bool isArrayReplaceNode = false;
    bool isArrayLengthNode  = false;
    bool hasArrayInputField = false;
};

struct DeferredPinState
{
    bool defaultPin  = false;
    bool startPin    = false;
    bool countPin    = false;
    bool aPin        = false;
    bool bPin        = false;
    bool durationPin = false;
    bool xPin        = false;
    bool yPin        = false;
    bool wPin        = false;
    bool hPin        = false;
    bool indexPin    = false;
};

namespace NodeRendererSpecialCases
{
// Everything a Handle* function needs to draw or modify a field.
// See nodeRenderer.h for the full guide on adding custom rendering.
struct FieldRenderContext
{
    VisualNode& node;
    const std::vector<VisualNode>* allNodes;

    const NodeRenderFlags& flags;       // node type / style classification
    bool& changed;                      // set true when a field value is modified
    bool& drawNodeColorTextChanged;     // set true when the color text field changes
    DeferredPinState& deferred;         // tracks which deferred input pins were drawn

    std::function<void(NodeField&, const char*)>        drawFieldWithConnectionRule;
    std::function<void(NodeField&, const char*, bool*)> drawDeferredPinAndField;
    std::function<bool(const char*)>                    isInputPinConnected;
};

bool HandleVariableField(NodeField& field, FieldRenderContext& context)
{
    if (context.flags.isGetVariable && field.name == "Type")
        return true;

    if (context.flags.isGetVariable && field.name == "Default")
        return true;

    if (context.flags.isGetVariable && field.name == "Name")
    {
        std::vector<std::string> setVariableNames;

        if (context.allNodes)
        {
            for (const VisualNode& other : *context.allNodes)
            {
                const NodeDescriptor* otherDesc = NodeRegistry::Get().Find(other.nodeType);
                if (!other.alive || !otherDesc || otherDesc->renderStyle != NodeRenderStyle::Variable)
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
            if (!field.value.empty())
            {
                field.value.clear();
                context.changed = true;
            }
            ImGui::TextUnformatted("Variable");
            ImGui::SameLine();
            ImGui::TextDisabled("(none)");
        }
        else
        {
            if (std::find(setVariableNames.begin(), setVariableNames.end(), field.value) == setVariableNames.end())
            {
                field.value = setVariableNames.front();
                context.changed = true;
            }

            ImGui::TextUnformatted("Variable");
            ImGui::SameLine();
            context.changed |= NodePopupComboDynamic("##GetVariableNodeCombo", field.value, setVariableNames, 110.0f);
        }
        return true;
    }

    if (context.flags.isSetVariable && field.name == "Default")
    {
        context.drawDeferredPinAndField(field, "Default", &context.deferred.defaultPin);
        return true;
    }

    return false;
}

bool HandleFlowStyleField(NodeField& field, FieldRenderContext& context)
{
    if (context.flags.isLoopNode && (field.name == "Start" || field.name == "Count"))
    {
        if (field.name == "Start")
            context.drawDeferredPinAndField(field, "Start", &context.deferred.startPin);
        else
            context.drawDeferredPinAndField(field, "Count", &context.deferred.countPin);
        return true;
    }

    if (context.flags.isBinaryDefaultNode && (field.name == "A" || field.name == "B"))
    {
        if (field.name == "A")
            context.drawDeferredPinAndField(field, "A", &context.deferred.aPin);
        else
            context.drawDeferredPinAndField(field, "B", &context.deferred.bPin);
        return true;
    }

    if (context.flags.isUnaryDefaultNode && field.name == "A")
    {
        context.drawDeferredPinAndField(field, "A", &context.deferred.aPin);
        return true;
    }

    if (context.flags.isDelayNode && field.name == "Duration")
    {
        context.drawDeferredPinAndField(field, "Duration", &context.deferred.durationPin);
        return true;
    }

    return false;
}

bool HandleArrayStyleField(NodeField& field, FieldRenderContext& context)
{
    if (context.flags.hasArrayInputField && field.name == "Array")
    {
        const bool pinConnected = context.isInputPinConnected("Array");
        if (pinConnected)
            DrawArrayFieldPreviewReadOnly(field, 3);
        else
            context.changed |= DrawField(field);
        return true;
    }

    if (context.flags.isArrayIndexNode && field.name == "Index")
    {
        context.drawDeferredPinAndField(field, "Index", &context.deferred.indexPin);
        return true;
    }

    if ((context.flags.isArrayAddNode && field.name == "Add Type")
        || (context.flags.isArrayReplaceNode && field.name == "Replace Type"))
    {
        static const std::vector<std::string> kAddTypeItems = {
            "Number", "Boolean", "String", "Array"
        };

        const bool replaceMode = context.flags.isArrayReplaceNode;
        ImGui::TextUnformatted(replaceMode ? "Replace Type" : "Add Type");
        ImGui::SameLine();
        const bool typeChanged = NodePopupComboDynamic(
            replaceMode ? "##ArrayReplaceTypeDropBar" : "##ArrayAddTypeDropBar",
            field.value,
            kAddTypeItems,
            110.0f
        );
        if (typeChanged)
        {
            context.changed = true;

            NodeField* typedValueField = FindFieldByName(context.node, replaceMode ? "Replace Value" : "Add Value");
            if (typedValueField)
            {
                const PinType resolved = ValuePinTypeFromString(field.value, PinType::Number);
                typedValueField->valueType = resolved;

                if (resolved == PinType::Boolean)
                    typedValueField->value = "false";
                else if (resolved == PinType::Number)
                    typedValueField->value = "0.0";
                else if (resolved == PinType::String)
                    typedValueField->value = "";
                else if (resolved == PinType::Array)
                    typedValueField->value = "[]";
            }
        }
        return true;
    }

    if ((context.flags.isArrayAddNode && field.name == "Add Value")
        || (context.flags.isArrayReplaceNode && field.name == "Replace Value"))
    {
        context.drawFieldWithConnectionRule(field, "Value");
        return true;
    }

    return false;
}

bool HandleDrawStyleField(NodeField& field, FieldRenderContext& context)
{
    if (context.flags.isDrawNode &&
        (field.name == "X" || field.name == "Y" || field.name == "W" || field.name == "H"))
    {
        if (field.name == "X") context.drawDeferredPinAndField(field, "X", &context.deferred.xPin);
        else if (field.name == "Y") context.drawDeferredPinAndField(field, "Y", &context.deferred.yPin);
        else if (field.name == "W") context.drawDeferredPinAndField(field, "W", &context.deferred.wPin);
        else if (field.name == "H") context.drawDeferredPinAndField(field, "H", &context.deferred.hPin);
        return true;
    }

    if (context.flags.isDrawNode && field.name == "Color")
    {
        ImGui::Text("%s", field.name.c_str());
        ImGui::SameLine();
        ImGui::PushItemWidth(100.0f);

        char buf[128] = {};
        std::snprintf(buf, sizeof(buf), "%s", field.value.c_str());
        if (ImGui::InputText("##value", buf, sizeof(buf)))
        {
            field.value = buf;
            context.changed = true;
            context.drawNodeColorTextChanged = true;
        }
        ImGui::PopItemWidth();
        return true;
    }

    if (context.flags.isDrawNode &&
        (field.name == "R" || field.name == "G" || field.name == "B"))
        return true;

    return false;
}

bool HandleStructStyleField(NodeField& field, FieldRenderContext& context)
{
    if (context.flags.isStructCreateNode && field.name == "Schema Fields")
        return true;

    if (context.flags.isStructDefineNode && field.name == "Fields")
    {
        const int fieldCount = GetArrayItemCountCached(field.value);
        ImGui::TextUnformatted("Fields");
        ImGui::SameLine();
        ImGui::TextDisabled("[%d]", fieldCount);
        return true;
    }

    if (context.flags.isStructCreateNode && field.name != "Struct Name")
    {
        context.drawDeferredPinAndField(field, field.name.c_str(), nullptr);
        return true;
    }

    return false;
}

bool HandleCallFunctionField(NodeField& field, FieldRenderContext& context)
{
    if (context.node.nodeType != NodeType::CallFunction)
        return false;

    if (field.name != "Name")
        return false;

    std::vector<std::string> functionNames;

    if (context.allNodes)
    {
        for (const VisualNode& other : *context.allNodes)
        {
            if (!other.alive || other.nodeType != NodeType::Function)
                continue;

            for (const NodeField& of : other.fields)
            {
                if (of.name == "Name" && !of.value.empty())
                {
                    if (std::find(functionNames.begin(), functionNames.end(), of.value) == functionNames.end())
                        functionNames.push_back(of.value);
                    break;
                }
            }
        }
    }

    if (functionNames.empty())
    {
        ImGui::TextUnformatted("Function");
        ImGui::SameLine();
        ImGui::TextDisabled("(none)");
    }
    else
    {
        if (std::find(functionNames.begin(), functionNames.end(), field.value) == functionNames.end())
        {
            field.value = functionNames.front();
            context.changed = true;
        }

        ImGui::TextUnformatted("Function");
        ImGui::SameLine();
        context.changed |= NodePopupComboDynamic(
            "##CallFunctionCombo", field.value, functionNames, 110.0f);
    }

    return true;
}

bool HandleCustomFieldRendering(NodeField& field, FieldRenderContext& context)
{
    return HandleVariableField(field, context)
        || HandleFlowStyleField(field, context)
        || HandleArrayStyleField(field, context)
        || HandleDrawStyleField(field, context)
        || HandleStructStyleField(field, context)
        || HandleCallFunctionField(field, context);  // LISÄÄ
}
}

namespace
{
NodeRenderFlags BuildNodeRenderFlags(const VisualNode& n, NodeRenderStyle renderStyle)
{
    auto findFieldValue = [&](const char* name) -> const std::string*
    {
        for (const NodeField& f : n.fields)
            if (f.name == name)
                return &f.value;
        return nullptr;
    };

    const std::string* variant = findFieldValue("Variant");

    NodeRenderFlags flags;
    flags.isGetVariable = (n.nodeType == NodeType::Variable && variant && *variant == "Get");
    flags.isSetVariable = (n.nodeType == NodeType::Variable && variant && *variant == "Set");
    flags.isForEachNode = (renderStyle == NodeRenderStyle::ForEach);
    flags.isLoopNode = (renderStyle == NodeRenderStyle::Loop);
    flags.isBinaryDefaultNode = (renderStyle == NodeRenderStyle::Binary);
    flags.isUnaryDefaultNode = (renderStyle == NodeRenderStyle::Unary);
    flags.isDelayNode = (renderStyle == NodeRenderStyle::Delay);
    flags.isDrawNode = (renderStyle == NodeRenderStyle::Draw);
    flags.isStructDefineNode = (renderStyle == NodeRenderStyle::StructDefine);
    flags.isStructCreateNode = (renderStyle == NodeRenderStyle::StructCreate);
    flags.isArrayIndexNode = (renderStyle == NodeRenderStyle::Array);
    flags.isArrayAddNode = (renderStyle == NodeRenderStyle::Array && n.nodeType == NodeType::ArrayAddAt);
    flags.isArrayReplaceNode = (renderStyle == NodeRenderStyle::Array && n.nodeType == NodeType::ArrayReplaceAt);
    flags.isArrayLengthNode = (renderStyle == NodeRenderStyle::Array && n.nodeType == NodeType::ArrayLength);
    flags.hasArrayInputField = (flags.isForEachNode || flags.isArrayIndexNode || flags.isArrayLengthNode);

    return flags;
}

void DrawDeferredPinsIfMissing(const NodeRenderFlags& flags,
                               const DeferredPinState& deferred,
                               const std::function<void(const char*)>& drawDeferredPinByName)
{
    if (flags.isSetVariable && !deferred.defaultPin)
        drawDeferredPinByName("Default");

    if (flags.isLoopNode)
    {
        if (!deferred.startPin) drawDeferredPinByName("Start");
        if (!deferred.countPin) drawDeferredPinByName("Count");
    }

    if (flags.isBinaryDefaultNode)
    {
        if (!deferred.aPin) drawDeferredPinByName("A");
        if (!deferred.bPin) drawDeferredPinByName("B");
    }

    if (flags.isUnaryDefaultNode && !deferred.aPin)
        drawDeferredPinByName("A");

    if (flags.isDelayNode && !deferred.durationPin)
        drawDeferredPinByName("Duration");

    if (flags.isArrayIndexNode && !deferred.indexPin)
        drawDeferredPinByName("Index");

    if (flags.isDrawNode)
    {
        if (!deferred.xPin) drawDeferredPinByName("X");
        if (!deferred.yPin) drawDeferredPinByName("Y");
        if (!deferred.wPin) drawDeferredPinByName("W");
        if (!deferred.hPin) drawDeferredPinByName("H");
    }
}

bool DrawSequenceStyleControls(VisualNode& n, IdGen* idGen)
{
    if (!idGen)
        return false;

    bool changed = false;
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

    if (ImGui::SmallButton("- Then"))
    {
        if (n.outPins.size() > 1)
        {
            n.outPins.pop_back();
            changed = true;
        }
    }

    return changed;
}

bool DrawFunctionStyleControls(VisualNode& n, IdGen* idGen)
{
    if (!idGen)
        return false;

    bool changed = false;

    if (ImGui::SmallButton("+ Param"))
    {
        int paramIndex = 0;
        for (const NodeField& f : n.fields)
            if (f.name.rfind("Param", 0) == 0)
                paramIndex++;

        n.fields.push_back(NodeField{
            "Param" + std::to_string(paramIndex),
            PinType::String,
            "param" + std::to_string(paramIndex)
            });
        changed = true;
    }

    if (ImGui::SmallButton("- Param"))
    {
        for (int i = static_cast<int>(n.fields.size()) - 1; i >= 0; --i)
        {
            if (n.fields[i].name.rfind("Param", 0) == 0)
            {
                n.fields.erase(n.fields.begin() + i);
                changed = true;
                break;
            }
        }
    }

    return changed;
}
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
    const NodeDescriptor* desc = NodeRegistry::Get().Find(n.nodeType);
    const NodeRenderStyle renderStyle = desc ? desc->renderStyle : NodeRenderStyle::Default;
    const float contentWidth = MeasureNodeContentWidth(n, desc);

    ed::BeginNode(n.id);

    if (!n.positioned)
    {
        // Apply loaded/spawned position only after node exists in the editor.
        // Doing this before BeginNode can be ignored by node-editor for fresh
        // nodes, which then leaves nodes stacked at default position.
        ed::SetNodePosition(n.id, n.initialPos);
        n.positioned = true;
    }

    const float headerHeight = 30.0f;
    const float headerPaddingX = 6.0f;
    const float headerInsetX = 12.0f;
    const float headerInsetY = 4.0f;
    const float headerWidth = contentWidth + headerPaddingX * 2.0f;
    const ImVec2 headerMin = ImGui::GetCursorScreenPos();

    // Match header fill with actual node body margins using node-editor style
    // padding, so all node types align evenly instead of fixed magic expansion.
    const auto& nodeStyle = ed::GetStyle();
    const float headerLeftInset = nodeStyle.NodePadding.x;
    const float headerRightInset = nodeStyle.NodePadding.z;
    const ImVec2 headerDrawMin(headerMin.x - headerLeftInset, headerMin.y);
    const float headerDrawWidth = headerWidth + headerLeftInset + headerRightInset;
    const ImVec2 headerDrawMax(headerDrawMin.x + headerDrawWidth, headerMin.y + headerHeight);
    const ImVec2 headerColorMin(headerDrawMin.x + headerInsetX, headerDrawMin.y + headerInsetY);
    const ImVec2 headerColorMax(headerDrawMax.x - headerInsetX, headerDrawMax.y - headerInsetY);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        headerColorMin,
        headerColorMax,
        ImGui::GetColorU32(GetNodeCategoryHeaderColor(desc)),
        4.0f
    );

    ImFont* titleFont = ImGui::GetFont();
    const float titleFontSize = ImGui::GetFontSize() + 3.0f;
    const ImVec2 titleSize = titleFont->CalcTextSizeA(titleFontSize, FLT_MAX, 0.0f, n.title.c_str());
    const float titleX = headerColorMin.x + std::max(0.0f, (headerColorMax.x - headerColorMin.x - titleSize.x) * 0.5f);
    const float titleY = headerColorMin.y + std::max(0.0f, (headerColorMax.y - headerColorMin.y - titleSize.y) * 0.5f);
    drawList->AddText(titleFont, titleFontSize, ImVec2(titleX, titleY), ImGui::GetColorU32(colors::textPrimary), n.title.c_str());
    ImGui::SetCursorScreenPos(ImVec2(headerMin.x, headerMin.y + headerHeight));
    // Reserve horizontal space so node width matches measured content width.
    ImGui::Dummy(ImVec2(headerWidth, 0.0f));
    ImGui::Spacing();

    bool changed = false;
    const NodeRenderFlags renderFlags = BuildNodeRenderFlags(n, renderStyle);
    bool drawNodeColorTextChanged = false;
    DeferredPinState deferredPins;

    // Helper: draw a single named input pin (used by deferred-pin logic below).
    auto drawDeferredPinByName = [&](const char* pinName)
    {
        for (const Pin& pin : n.inPins)
        {
            if (pin.name == pinName)
            {
                DrawPin(pin, contentWidth, allLinks);
                return;
            }
        }
    };

    // ── Phase 2: Non-deferred input pins ─────────────────────────────────────
    // Pins listed in NodeDescriptor::deferredInputPins are skipped here; they
    // will be drawn inline beside their matching field widget in phase 3.
    for (const Pin& pin : n.inPins)
    {
        if (IsDeferredInputPin(desc, pin))
            continue;
        DrawPin(pin, contentWidth, allLinks);
    }

    if (!n.fields.empty())
    {
        // Returns true when the named input pin has an active link.
        // Used by field handlers to switch between editable and read-only.
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

        auto drawFieldWithConnectionRule = [&](NodeField& field, const char* pinName)
        {
            if (isInputPinConnected(pinName))
                DrawFieldReadOnly(field, FieldWidgetLayout::Compact);
            else
                changed |= DrawField(field);
        };

        auto drawDeferredPinAndField = [&](NodeField& field, const char* pinName, bool* drewFlag = nullptr)
        {
            drawDeferredPinByName(pinName);
            if (drewFlag)
                *drewFlag = true;
            drawFieldWithConnectionRule(field, pinName);
        };

        NodeRendererSpecialCases::FieldRenderContext specialCtx{
            n,
            allNodes,
            renderFlags,
            changed,
            drawNodeColorTextChanged,
            deferredPins,
            drawFieldWithConnectionRule,
            drawDeferredPinAndField,
            isInputPinConnected
        };

        // ── Phase 3: Fields ───────────────────────────────────────────────────
        // Each field is offered to HandleCustomFieldRendering first (see
        // nodeRenderer.h for how to add new custom behavior). If no handler
        // claims the field, it falls through to the generic DrawField widget.
        ImGui::Spacing();
        ImGui::PushID((int)n.id.Get());
        for (NodeField& field : n.fields)
        {
            if (field.name == "Variant")
                continue; // internal bookkeeping — never shown in the node body

            if (NodeRendererSpecialCases::HandleCustomFieldRendering(field, specialCtx))
                continue;

            // Generic fallback: if a field has a same-named input pin and that
            // pin is connected, show the value as read-only.
            if (isInputPinConnected(field.name.c_str()))
                DrawFieldReadOnly(field, FieldWidgetLayout::Compact);
            else
                changed |= DrawField(field);
        }
        ImGui::PopID();
        ImGui::Spacing();
    }

    // ── Phase 4: Deferred input pins ─────────────────────────────────────────
    // Some pins are drawn inline with their matching field (phase 3). Any that
    // weren't reached there are drawn here as plain pins below the field rows.
    DrawDeferredPinsIfMissing(renderFlags, deferredPins, drawDeferredPinByName);

    if (renderFlags.isDrawNode)
    {
        changed |= ClampDrawNodeGeometryFields(n);
        changed |= SyncDrawNodeColorFields(n, drawNodeColorTextChanged);
    }

    if (changed)
        GraphSerializer::ApplyConstantTypeFromFields(n, /*resetValueOnTypeChange=*/true);

    if (renderStyle == NodeRenderStyle::Sequence)
        changed |= DrawSequenceStyleControls(n, idGen);

    if (n.nodeType == NodeType::Function)
        changed |= DrawFunctionStyleControls(n, idGen);

    // ── Phase 5: Output pins ──────────────────────────────────────────────────
    for (const Pin& pin : n.outPins)
        DrawPin(pin, contentWidth, allLinks);

    ed::EndNode();

    return changed;
}
