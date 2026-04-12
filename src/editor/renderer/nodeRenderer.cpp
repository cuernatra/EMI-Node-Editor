#include "nodeRenderer.h"

#include "fieldWidgetRenderer.h"
#include "pinRenderer.h"

#include "app/constants.h"
#include "core/registry/nodeRegistry.h"
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
#include <unordered_set>
#include <utility>

namespace ed = ax::NodeEditor;

namespace
{
ImVec4 GetNodeCategoryHeaderColor(const NodeDescriptor* desc)
{
    const std::string& category = desc ? desc->category : std::string{};
    const auto rgba = Settings::GetNodeHeaderColor(category);
    return ImVec4(rgba[0], rgba[1], rgba[2], rgba[3]);
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

bool IsDeferredInputPin(const VisualNode& node, const NodeDescriptor* desc, const Pin& pin)
{
    if (!desc || !pin.isInput)
        return false;

    if (std::find(desc->deferredInputPins.begin(), desc->deferredInputPins.end(), pin.name) != desc->deferredInputPins.end())
        return true;

    const bool wildcard = std::find(desc->deferredInputPins.begin(), desc->deferredInputPins.end(), "*") != desc->deferredInputPins.end();
    if (!wildcard)
        return false;

    return FindFieldByName(node, pin.name.c_str()) != nullptr;
}

float MeasureFieldWidth(NodeRenderStyle renderStyle, const NodeField& field)
{
    const float labelWidth  = ImGui::CalcTextSize(field.name.c_str()).x;
    float widgetWidth = 82.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;

    // Keep Constant nodes narrower in graph view.
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

    // Make Rect Click and Sequence nodes just a bit wider so title/text spacing is cleaner.
    if (n.nodeType == NodeType::PreviewPickRect || n.nodeType == NodeType::Sequence)
        maxWidth += 14.0f;

    // The header title is rendered at a larger font size (fontSize + 3) inside a
    // colour bar that is inset by headerInsetX on both sides from the draw bounds.
    // Ensure contentWidth is large enough that the colour bar can contain the title.
    //   colorBarWidth = contentWidth + headerPaddingX*2 + nodePad.x + nodePad.z - headerInsetX*2
    // Solving for the minimum contentWidth:
    //   contentWidth >= titleLargeWidth - headerPaddingX*2 - nodePad.x - nodePad.z + headerInsetX*2
    {
        constexpr float headerPaddingX = 6.0f;
        constexpr float headerInsetX   = 12.0f;
        const auto&     nodePad        = ed::GetStyle().NodePadding;
        const float     titleLargeW    = ImGui::GetFont()->CalcTextSizeA(
                                             ImGui::GetFontSize() + 3.0f, FLT_MAX, 0.0f,
                                             n.title.c_str()).x;
        const float minForTitle = titleLargeW
                                  - headerPaddingX * 2.0f
                                  - nodePad.x - nodePad.z
                                  + headerInsetX * 2.0f;
        maxWidth = std::max(maxWidth, minForTitle);
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

// Keep these structs above NodeRendererSpecialCases so FieldRenderContext can
// reference them directly.
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
    bool isStructReadNode   = false;
    bool isStructWriteNode  = false;
    bool isArrayIndexNode   = false;
    bool isArrayAddNode     = false;
    bool isArrayReplaceNode = false;
    bool isArrayLengthNode  = false;
    bool hasArrayInputField = false;

    bool isFunctionNode     = false;
    bool isCallFunctionNode = false;
};

struct DeferredPinState
{
    std::unordered_set<std::string> drawnPins;
};

namespace NodeRendererSpecialCases
{
// Shared data/helpers passed to custom field handlers.
struct FieldRenderContext
{
    VisualNode& node;
    const std::vector<VisualNode>* allNodes;

    const NodeRenderFlags& flags;       // What kind of node we are drawing.
    bool& changed;                      // Set true when any UI edit changes data.
    bool& drawNodeColorTextChanged;     // Tracks direct edits to Color text field.
    DeferredPinState& deferred;         // Tracks deferred pins already drawn inline.

    std::function<void(NodeField&, const char*)>        drawFieldWithConnectionRule;
    std::function<void(NodeField&, const char*)>        drawDeferredPinAndField;
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

    return false;
}

bool HandleFlowStyleField(NodeField& field, FieldRenderContext& context)
{
    if (context.flags.isCallFunctionNode && field.name == "Name")
    {
        std::vector<std::string> functionNames;

        if (context.allNodes)
        {
            for (const VisualNode& other : *context.allNodes)
            {
                if (!other.alive || other.nodeType != NodeType::Function)
                    continue;

                const NodeField* nameField = FindFieldByName(other, "Name");
                if (!nameField || nameField->value.empty())
                    continue;

                if (std::find(functionNames.begin(), functionNames.end(), nameField->value) == functionNames.end())
                    functionNames.push_back(nameField->value);
            }
        }

        if (functionNames.empty())
        {
            if (!field.value.empty())
            {
                field.value.clear();
                context.changed = true;
            }
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
            context.changed |= NodePopupComboDynamic("##CallFunctionCombo", field.value, functionNames, 110.0f);
        }

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
    auto resolveStructFieldTypeFromSchema = [](const std::string& schemaText, const std::string& wantedField) -> PinType
    {
        if (wantedField.empty())
            return PinType::Any;

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

            if (name != wantedField)
                continue;

            const std::string typeName = TrimArrayToken(
                (sep == std::string::npos) ? std::string("Any") : raw.substr(sep + 1)
            );
            return ValuePinTypeFromString(typeName, PinType::Any);
        }

        return PinType::Any;
    };

    auto collectDefinedStructNames = [&]() -> std::vector<std::string>
    {
        std::vector<std::string> names;
        if (!context.allNodes)
            return names;

        for (const VisualNode& other : *context.allNodes)
        {
            if (!other.alive || other.nodeType != NodeType::StructDefine)
                continue;

            const NodeField* nameField = FindFieldByName(other, "Struct Name");
            const std::string value = (nameField && !nameField->value.empty()) ? nameField->value : "test";
            if (std::find(names.begin(), names.end(), value) == names.end())
                names.push_back(value);
        }
        return names;
    };

    auto collectStructInstanceNames = [&]() -> std::vector<std::string>
    {
        std::vector<std::string> names;
        if (!context.allNodes)
            return names;

        for (const VisualNode& other : *context.allNodes)
        {
            if (!other.alive || other.nodeType != NodeType::StructCreate)
                continue;

            const NodeField* instanceField = FindFieldByName(other, "Instance Name");
            const std::string value = (instanceField && !instanceField->value.empty()) ? instanceField->value : "structItem";
            if (std::find(names.begin(), names.end(), value) == names.end())
                names.push_back(value);
        }

        return names;
    };

    auto countStructDefineName = [&](const std::string& candidate) -> int
    {
        if (!context.allNodes || candidate.empty())
            return 0;

        int count = 0;
        for (const VisualNode& other : *context.allNodes)
        {
            if (!other.alive || other.nodeType != NodeType::StructDefine)
                continue;

            const NodeField* nameField = FindFieldByName(other, "Struct Name");
            const std::string value = nameField ? nameField->value : std::string();
            if (value == candidate)
                ++count;
        }
        return count;
    };

    auto countStructInstanceName = [&](const std::string& candidate) -> int
    {
        if (!context.allNodes || candidate.empty())
            return 0;

        int count = 0;
        for (const VisualNode& other : *context.allNodes)
        {
            if (!other.alive || other.nodeType != NodeType::StructCreate)
                continue;

            const NodeField* nameField = FindFieldByName(other, "Instance Name");
            const std::string value = nameField ? nameField->value : std::string();
            if (value == candidate)
                ++count;
        }
        return count;
    };

    if (context.flags.isStructDefineNode && field.name == "Struct Name")
    {
        context.changed |= DrawField(field);
        if (!field.value.empty() && countStructDefineName(field.value) > 1)
            ImGui::TextColored(colors::error, "Duplicate Struct Name");
        return true;
    }

    if (context.flags.isStructCreateNode && field.name == "Instance Name")
    {
        context.changed |= DrawField(field);
        if (!field.value.empty() && countStructInstanceName(field.value) > 1)
            ImGui::TextColored(colors::error, "Duplicate Instance Name");
        return true;
    }

    if ((context.flags.isStructCreateNode
        || context.flags.isStructReadNode
        || context.flags.isStructWriteNode)
        && field.name == "Schema Fields")
        return true;

    if ((context.flags.isStructCreateNode || context.flags.isStructReadNode || context.flags.isStructWriteNode)
        && field.name == "Struct Name")
    {
        std::vector<std::string> names = collectDefinedStructNames();
        if (!names.empty())
        {
            if (std::find(names.begin(), names.end(), field.value) == names.end())
            {
                field.value = names.front();
                context.changed = true;
            }
            ImGui::TextUnformatted("Struct");
            ImGui::SameLine();
            const char* comboId = context.flags.isStructCreateNode
                ? "##StructCreateStructNameCombo"
                : (context.flags.isStructReadNode
                    ? "##StructReadStructNameCombo"
                    : "##StructWriteStructNameCombo");
            context.changed |= NodePopupComboDynamic(comboId, field.value, names, 120.0f);
        }
        else
        {
            ImGui::TextUnformatted("Struct");
            ImGui::SameLine();
            ImGui::TextDisabled("(none)");
        }
        return true;
    }

    if ((context.flags.isStructReadNode || context.flags.isStructWriteNode) && field.name == "Instance Name")
    {
        std::vector<std::string> names = collectStructInstanceNames();
        if (!names.empty())
        {
            if (std::find(names.begin(), names.end(), field.value) == names.end())
            {
                field.value = names.front();
                context.changed = true;
            }
            ImGui::TextUnformatted("Instance");
            ImGui::SameLine();
            context.changed |= NodePopupComboDynamic(
                context.flags.isStructReadNode ? "##StructReadInstanceNameCombo" : "##StructWriteInstanceNameCombo",
                field.value,
                names,
                120.0f
            );
        }
        else
        {
            context.changed |= DrawField(field);
        }
        return true;
    }

    if ((context.flags.isStructReadNode || context.flags.isStructWriteNode) && field.name == "Field Name")
    {
        const NodeField* schemaField = FindFieldByName(context.node, "Schema Fields");
        const std::string schemaText = schemaField ? schemaField->value : "[]";
        const std::vector<std::string>& names = GetStructFieldNamesCached(schemaField ? schemaField->value : "[]");
        bool selectionChanged = false;
        if (!names.empty())
        {
            if (std::find(names.begin(), names.end(), field.value) == names.end())
            {
                field.value = names.front();
                context.changed = true;
                selectionChanged = true;
            }
            ImGui::TextUnformatted("Field");
            ImGui::SameLine();
            const bool comboChanged = NodePopupComboDynamic(
                context.flags.isStructReadNode ? "##StructReadFieldNameCombo" : "##StructWriteFieldNameCombo",
                field.value,
                names,
                120.0f
            );
            if (comboChanged)
            {
                context.changed = true;
                selectionChanged = true;
            }
        }
        else
        {
            context.changed |= DrawField(field);
        }

        // For Struct Write, update Value field type immediately when Field changes,
        // so UI and fallback compile literal use the selected field's type.
        if (context.flags.isStructWriteNode)
        {
            const PinType targetType = resolveStructFieldTypeFromSchema(schemaText, field.value);
            if (NodeField* valueField = FindFieldByName(context.node, "Value"))
            {
                if (valueField->valueType != targetType)
                {
                    valueField->valueType = targetType;
                    selectionChanged = true;
                }

                // Keep current typed value when possible, only normalize obvious invalids.
                if (targetType == PinType::Number)
                {
                    char* end = nullptr;
                    const double parsed = std::strtod(valueField->value.c_str(), &end);
                    if (!(end && *end == '\0'))
                    {
                        valueField->value = "0.0";
                        selectionChanged = true;
                    }
                    else
                    {
                        (void)parsed;
                    }
                }
                else if (targetType == PinType::Boolean)
                {
                    const bool isTrue = (valueField->value == "true" || valueField->value == "True" || valueField->value == "1");
                    const bool isFalse = (valueField->value == "false" || valueField->value == "False" || valueField->value == "0");
                    if (!isTrue && !isFalse)
                    {
                        valueField->value = "false";
                        selectionChanged = true;
                    }
                }
                else if ((targetType == PinType::Array || targetType == PinType::Struct) && valueField->value.empty())
                {
                    valueField->value = "[]";
                    selectionChanged = true;
                }
            }
        }

        if (selectionChanged)
            context.changed = true;

        return true;
    }

    if (context.flags.isStructWriteNode && field.name == "Value")
    {
        context.drawFieldWithConnectionRule(field, "Value");
        return true;
    }

    if (context.flags.isStructDefineNode && field.name == "Fields")
    {
        const int fieldCount = GetArrayItemCountCached(field.value);
        ImGui::TextUnformatted("Fields");
        ImGui::SameLine();
        ImGui::TextDisabled("[%d]", fieldCount);
        return true;
    }

    return false;
}

bool HandleCustomFieldRendering(NodeField& field, FieldRenderContext& context)
{
    return HandleVariableField(field, context)
        || HandleFlowStyleField(field, context)
        || HandleArrayStyleField(field, context)
        || HandleDrawStyleField(field, context)
        || HandleStructStyleField(field, context);
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
    flags.isStructReadNode   = (renderStyle == NodeRenderStyle::StructRead);
    flags.isStructWriteNode  = (renderStyle == NodeRenderStyle::StructWrite);
    flags.isArrayIndexNode = (renderStyle == NodeRenderStyle::Array);
    flags.isArrayAddNode = (renderStyle == NodeRenderStyle::Array && n.nodeType == NodeType::ArrayAddAt);
    flags.isArrayReplaceNode = (renderStyle == NodeRenderStyle::Array && n.nodeType == NodeType::ArrayReplaceAt);
    flags.isArrayLengthNode = (renderStyle == NodeRenderStyle::Array && n.nodeType == NodeType::ArrayLength);
    flags.hasArrayInputField = (flags.isForEachNode || flags.isArrayIndexNode || flags.isArrayLengthNode);

    flags.isFunctionNode = (n.nodeType == NodeType::Function);
    flags.isCallFunctionNode = (n.nodeType == NodeType::CallFunction);

    return flags;
}

void DrawDeferredPinsIfMissing(const VisualNode& n,
                               const NodeDescriptor* desc,
                               const DeferredPinState& deferred,
                               const std::function<void(const char*)>& drawDeferredPinByName)
{
    for (const Pin& pin : n.inPins)
    {
        if (!IsDeferredInputPin(n, desc, pin))
            continue;

        if (deferred.drawnPins.find(pin.name) != deferred.drawnPins.end())
            continue;

        drawDeferredPinByName(pin.name.c_str());
    }
}

bool DrawSequenceStyleControls(VisualNode& n, IdGen* idGen)
{
    if (!idGen)
        return false;

    // These buttons can appear on multiple nodes; ensure unique ImGui IDs per node.
    ImGui::PushID((int)n.id.Get());
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

    ImGui::PopID();
    return changed;
}

bool DrawFunctionParamControls(VisualNode& n)
{
    if (n.nodeType != NodeType::Function)
        return false;

    // These buttons can appear on multiple Function nodes; ensure unique ImGui IDs per node.
    ImGui::PushID((int)n.id.Get());
    bool changed = false;

    if (ImGui::SmallButton("+ Param"))
    {
        int nextIndex = 0;
        for (const NodeField& f : n.fields)
        {
            if (f.name.rfind("Param", 0) != 0)
                continue;

            const std::string suffix = f.name.substr(5);
            if (suffix.empty())
                continue;

            bool allDigits = true;
            for (char ch : suffix)
                allDigits = allDigits && std::isdigit(static_cast<unsigned char>(ch));
            if (!allDigits)
                continue;

            try
            {
                const int idx = std::stoi(suffix);
                nextIndex = std::max(nextIndex, idx + 1);
            }
            catch (...) {}
        }

        const std::string paramName = "param" + std::to_string(nextIndex);
        n.fields.push_back(NodeField{ "Param" + std::to_string(nextIndex), PinType::String, paramName });
        changed = true;
    }

    if (ImGui::SmallButton("- Param"))
    {
        int bestIndex = -1;
        int bestFieldIndex = -1;

        for (int i = 0; i < static_cast<int>(n.fields.size()); ++i)
        {
            const NodeField& f = n.fields[static_cast<size_t>(i)];
            if (f.name.rfind("Param", 0) != 0)
                continue;

            const std::string suffix = f.name.substr(5);
            if (suffix.empty())
                continue;

            bool allDigits = true;
            for (char ch : suffix)
                allDigits = allDigits && std::isdigit(static_cast<unsigned char>(ch));
            if (!allDigits)
                continue;

            try
            {
                const int idx = std::stoi(suffix);
                if (idx > bestIndex)
                {
                    bestIndex = idx;
                    bestFieldIndex = i;
                }
            }
            catch (...) {}
        }

        if (bestFieldIndex >= 0)
        {
            n.fields.erase(n.fields.begin() + bestFieldIndex);
            changed = true;
        }
    }

    ImGui::PopID();
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
        // Set initial position after BeginNode.
        // If set too early, node-editor may ignore it for fresh nodes.
        ed::SetNodePosition(n.id, n.initialPos);
        n.positioned = true;
    }

    const float headerHeight = 30.0f;
    const float headerPaddingX = 6.0f;
    const float headerInsetX = 12.0f;
    const float headerInsetY = 4.0f;
    const float headerWidth = contentWidth + headerPaddingX * 2.0f;
    const ImVec2 headerMin = ImGui::GetCursorScreenPos();

    // Use node-editor padding values so header/body edges line up cleanly.
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
    // Reserve width so the node uses the measured content width.
    ImGui::Dummy(ImVec2(headerWidth, 0.0f));
    ImGui::Spacing();

    bool changed = false;
    const NodeRenderFlags renderFlags = BuildNodeRenderFlags(n, renderStyle);
    bool drawNodeColorTextChanged = false;
    DeferredPinState deferredPins;

    // Helper: draw one input pin by name.
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

    // Phase 2: draw regular input pins.
    // Deferred pins are skipped here and drawn next to fields.
    for (const Pin& pin : n.inPins)
    {
        if (IsDeferredInputPin(n, desc, pin))
            continue;
        DrawPin(pin, contentWidth, allLinks);
    }

    if (!n.fields.empty())
    {
        // True when the input pin with this name is currently linked.
        // Linked pins make matching fields read-only.
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

        auto hasMatchingDeferredInputPin = [&](const char* fieldName) -> bool
        {
            for (const Pin& pin : n.inPins)
            {
                if (pin.name == fieldName && IsDeferredInputPin(n, desc, pin))
                    return true;
            }
            return false;
        };

        auto drawDeferredPinAndField = [&](NodeField& field, const char* pinName)
        {
            drawDeferredPinByName(pinName);
            deferredPins.drawnPins.insert(pinName);
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

        // Phase 3: draw fields.
        // Try custom rendering first; otherwise fall back to default field widget.
        ImGui::Spacing();
        ImGui::PushID((int)n.id.Get());
        for (NodeField& field : n.fields)
        {
            if (field.name == "Variant")
                continue; // Internal metadata field; hidden from node body.

            if (hasMatchingDeferredInputPin(field.name.c_str()))
            {
                drawDeferredPinAndField(field, field.name.c_str());
                continue;
            }

            if (NodeRendererSpecialCases::HandleCustomFieldRendering(field, specialCtx))
                continue;

            // Default behavior: same-name linked input => read-only field display.
            if (isInputPinConnected(field.name.c_str()))
                DrawFieldReadOnly(field, FieldWidgetLayout::Compact);
            else
                changed |= DrawField(field);
        }
        ImGui::PopID();
        ImGui::Spacing();
    }

    // Phase 4: draw any deferred pins that were not drawn beside fields.
    DrawDeferredPinsIfMissing(n, desc, deferredPins, drawDeferredPinByName);

    if (renderFlags.isDrawNode)
    {
        changed |= ClampDrawNodeGeometryFields(n);
        changed |= SyncDrawNodeColorFields(n, drawNodeColorTextChanged);
    }

    if (changed)
        GraphSerializer::ApplyConstantTypeFromFields(n, /*resetValueOnTypeChange=*/true);

    if (renderStyle == NodeRenderStyle::Sequence)
        changed |= DrawSequenceStyleControls(n, idGen);

    if (renderFlags.isFunctionNode)
        changed |= DrawFunctionParamControls(n);

    // Phase 5: draw output pins.
    for (const Pin& pin : n.outPins)
        DrawPin(pin, contentWidth, allLinks);

    ed::EndNode();

    return changed;
}
