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
static std::unordered_map<uint64_t, float> s_nodeWidthCache;

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

static void DrawReadOnlyField(const NodeField& field)
{
    ImGui::TextUnformatted(field.name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", field.value.c_str());
}

static void DrawReadOnlyArrayFieldPreview(const NodeField& field, int previewCount = 3)
{
    const std::vector<std::string> items = ParseArrayItemsForNodeView(field.value);

    ImGui::TextUnformatted(field.name.c_str());
    ImGui::SameLine();

    std::string preview = "[";
    const int visible = std::min(static_cast<int>(items.size()), std::max(previewCount, 0));
    for (int i = 0; i < visible; ++i)
    {
        if (i > 0)
            preview += ", ";

        std::string token = TrimArrayToken(items[static_cast<size_t>(i)]);
        if (token.size() > 12)
            token = token.substr(0, 12) + "...";
        preview += token;
    }

    if (static_cast<int>(items.size()) > visible)
    {
        if (visible > 0)
            preview += ", ";
        preview += "...";
    }
    preview += "]";

    ImGui::TextDisabled("%s [%d] %s", field.name.c_str(), static_cast<int>(items.size()), preview.c_str());
}

float MeasureFieldWidth(const VisualNode& node, const NodeField& field)
{
    const float labelWidth  = ImGui::CalcTextSize(field.name.c_str()).x;
    float widgetWidth = 82.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;

    // Keep Constant node much narrower in graph view.
    if (node.nodeType == NodeType::Constant)
    {
        widgetWidth = 58.0f;
        spacing = 4.0f;
    }

    return labelWidth + spacing + widgetWidth;
}

float MeasureNodeContentWidth(const VisualNode& n)
{
    const float iconWidth = 14.0f;
    const float pinGap    = 6.0f;
    const float pinRowPad = iconWidth + pinGap;

    float maxWidth = ImGui::CalcTextSize(n.title.c_str()).x;

    for (const Pin& pin : n.inPins)
        maxWidth = std::max(maxWidth, pinRowPad + ImGui::CalcTextSize(pin.name.c_str()).x);

    for (const Pin& pin : n.outPins)
        maxWidth = std::max(maxWidth, ImGui::CalcTextSize(pin.name.c_str()).x + pinGap + iconWidth);

    for (const NodeField& field : n.fields)
    {
        if (field.name == "Variant") continue;
        maxWidth = std::max(maxWidth, MeasureFieldWidth(n, field));
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

    const float contentWidth = MeasureNodeContentWidth(n);

    ed::BeginNode(n.id);

    const float headerHeight = 30.0f;
    const float headerPaddingX = 6.0f;
    const float headerInsetX = 12.0f;
    const float headerInsetY = 4.0f;
    const float headerWidth = contentWidth + headerPaddingX * 2.0f;
    const ImVec2 headerMin = ImGui::GetCursorScreenPos();
    const uint64_t nodeKey = static_cast<uint64_t>(n.id.Get());

    // Match header fill with actual node body margins using node-editor style
    // padding, so all node types align evenly instead of fixed magic expansion.
    const auto& nodeStyle = ed::GetStyle();
    const float headerLeftInset = nodeStyle.NodePadding.x;
    const float headerRightInset = nodeStyle.NodePadding.z;
    const ImVec2 headerDrawMin(headerMin.x - headerLeftInset, headerMin.y);
    float headerDrawWidth = headerWidth + headerLeftInset + headerRightInset;
    if (auto it = s_nodeWidthCache.find(nodeKey); it != s_nodeWidthCache.end() && it->second > 0.0f)
        headerDrawWidth = it->second;
    const ImVec2 headerDrawMax(headerDrawMin.x + headerDrawWidth, headerMin.y + headerHeight);
    const ImVec2 headerColorMin(headerDrawMin.x + headerInsetX, headerDrawMin.y + headerInsetY);
    const ImVec2 headerColorMax(headerDrawMax.x - headerInsetX, headerDrawMax.y - headerInsetY);

    const NodeDescriptor* desc = NodeRegistry::Get().Find(n.nodeType);

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
    const bool isForEachNode = (n.nodeType == NodeType::ForEach);
    const bool isLoopNode = (n.nodeType == NodeType::Loop);
    const bool isBinaryDefaultNode =
        (n.nodeType == NodeType::Operator || n.nodeType == NodeType::Comparison || n.nodeType == NodeType::Logic);
    const bool isUnaryDefaultNode = (n.nodeType == NodeType::Not);
    const bool isDelayNode = (n.nodeType == NodeType::Delay);
    const bool isDrawRectNode = (n.nodeType == NodeType::DrawRect);
    const bool isDrawGridNode = (n.nodeType == NodeType::DrawGrid);
    const bool isStructDefineNode = (n.nodeType == NodeType::StructDefine);
    const bool isStructCreateNode = (n.nodeType == NodeType::StructCreate);
    const bool isArrayIndexNode =
        (n.nodeType == NodeType::ArrayGetAt
         || n.nodeType == NodeType::ArrayAddAt
         || n.nodeType == NodeType::ArrayReplaceAt
         || n.nodeType == NodeType::ArrayRemoveAt);
    const bool isArrayAddNode = (n.nodeType == NodeType::ArrayAddAt);
    const bool isArrayReplaceNode = (n.nodeType == NodeType::ArrayReplaceAt);
    const bool isArrayLengthNode = (n.nodeType == NodeType::ArrayLength);
    const bool hasArrayInputFieldNode = (isForEachNode || isArrayIndexNode || isArrayLengthNode);
    bool drawNodeColorTextChanged = false;

    bool drewDeferredDefaultPin = false;
    bool drewDeferredStartPin = false;
    bool drewDeferredCountPin = false;
    bool drewDeferredAPin = false;
    bool drewDeferredBPin = false;
    bool drewDeferredDurationPin = false;
    bool drewDeferredXPin = false;
    bool drewDeferredYPin = false;
    bool drewDeferredWPin = false;
    bool drewDeferredHPin = false;
    bool drewDeferredIndexPin = false;

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

    auto shouldDeferInputPin = [&](const Pin& pin) -> bool
    {
        if (isSetVariable && pin.name == "Default")
            return true;

        if (isLoopNode && (pin.name == "Start" || pin.name == "Count"))
            return true;

        if (isBinaryDefaultNode && (pin.name == "A" || pin.name == "B"))
            return true;

        if (isUnaryDefaultNode && pin.name == "A")
            return true;

        if (isDelayNode && pin.name == "Duration")
            return true;

        if (isDrawRectNode &&
            (pin.name == "X" || pin.name == "Y" || pin.name == "W" || pin.name == "H"))
            return true;

        if (isDrawGridNode &&
            (pin.name == "X" || pin.name == "Y" || pin.name == "W" || pin.name == "H"))
            return true;

        if (isStructCreateNode && pin.name != "Struct")
            return true;

        if (isArrayIndexNode && pin.name == "Index")
            return true;

        return false;
    };

    for (const Pin& pin : n.inPins)
    {
        if (shouldDeferInputPin(pin))
            continue;
        DrawPin(pin, contentWidth, allLinks);
    }

    if (!n.fields.empty())
    {
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
                    if (!field.value.empty())
                    {
                        field.value.clear();
                        changed = true;
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
                drawDeferredPinByName("Default");
                drewDeferredDefaultPin = true;

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

            if (isLoopNode && (field.name == "Start" || field.name == "Count"))
            {
                if (field.name == "Start")
                {
                    drawDeferredPinByName("Start");
                    drewDeferredStartPin = true;
                }
                else
                {
                    drawDeferredPinByName("Count");
                    drewDeferredCountPin = true;
                }

                const bool pinConnected = isInputPinConnected(field.name.c_str());
                if (pinConnected)
                {
                    DrawReadOnlyField(field);
                }
                else
                {
                    changed |= DrawField(field);
                }
                continue;
            }

            if (isBinaryDefaultNode && (field.name == "A" || field.name == "B"))
            {
                if (field.name == "A")
                {
                    drawDeferredPinByName("A");
                    drewDeferredAPin = true;
                }
                else
                {
                    drawDeferredPinByName("B");
                    drewDeferredBPin = true;
                }

                const bool pinConnected = isInputPinConnected(field.name.c_str());
                if (pinConnected)
                    DrawReadOnlyField(field);
                else
                    changed |= DrawField(field);
                continue;
            }

            if (isUnaryDefaultNode && field.name == "A")
            {
                drawDeferredPinByName("A");
                drewDeferredAPin = true;

                const bool pinConnected = isInputPinConnected(field.name.c_str());
                if (pinConnected)
                    DrawReadOnlyField(field);
                else
                    changed |= DrawField(field);
                continue;
            }

            if (isDelayNode && field.name == "Duration")
            {
                drawDeferredPinByName("Duration");
                drewDeferredDurationPin = true;

                const bool pinConnected = isInputPinConnected(field.name.c_str());
                if (pinConnected)
                    DrawReadOnlyField(field);
                else
                    changed |= DrawField(field);
                continue;
            }

            if (hasArrayInputFieldNode && field.name == "Array")
            {
                const bool pinConnected = isInputPinConnected("Array");
                if (pinConnected)
                    DrawReadOnlyArrayFieldPreview(field, 3);
                else
                    changed |= DrawField(field);
                continue;
            }

            if (isArrayIndexNode && field.name == "Index")
            {
                drawDeferredPinByName("Index");
                drewDeferredIndexPin = true;

                const bool pinConnected = isInputPinConnected(field.name.c_str());
                if (pinConnected)
                    DrawReadOnlyField(field);
                else
                    changed |= DrawField(field);
                continue;
            }

            if ((isArrayAddNode && field.name == "Add Type")
                || (isArrayReplaceNode && field.name == "Replace Type"))
            {
                static const std::vector<std::string> kAddTypeItems = {
                    "Number", "Boolean", "String", "Array"
                };

                const bool replaceMode = isArrayReplaceNode;
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
                    changed = true;

                    NodeField* typedValueField = FindFieldByName(n, replaceMode ? "Replace Value" : "Add Value");
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
                continue;
            }

            if ((isArrayAddNode && field.name == "Add Value")
                || (isArrayReplaceNode && field.name == "Replace Value"))
            {
                const bool valueConnected = isInputPinConnected("Value");
                if (valueConnected)
                    DrawReadOnlyField(field);
                else
                    changed |= DrawField(field);
                continue;
            }

            if (isDrawRectNode &&
                (field.name == "X" || field.name == "Y" || field.name == "W" || field.name == "H"))
            {
                if (field.name == "X") { drawDeferredPinByName("X"); drewDeferredXPin = true; }
                else if (field.name == "Y") { drawDeferredPinByName("Y"); drewDeferredYPin = true; }
                else if (field.name == "W") { drawDeferredPinByName("W"); drewDeferredWPin = true; }
                else if (field.name == "H") { drawDeferredPinByName("H"); drewDeferredHPin = true; }

                const bool pinConnected = isInputPinConnected(field.name.c_str());
                if (pinConnected)
                    DrawReadOnlyField(field);
                else
                    changed |= DrawField(field);
                continue;
            }

            if (isDrawGridNode &&
                (field.name == "X" || field.name == "Y" || field.name == "W" || field.name == "H"))
            {
                if (field.name == "X") { drawDeferredPinByName("X"); drewDeferredXPin = true; }
                else if (field.name == "Y") { drawDeferredPinByName("Y"); drewDeferredYPin = true; }
                else if (field.name == "W") { drawDeferredPinByName("W"); drewDeferredWPin = true; }
                else if (field.name == "H") { drawDeferredPinByName("H"); drewDeferredHPin = true; }

                const bool pinConnected = isInputPinConnected(field.name.c_str());
                if (pinConnected)
                    DrawReadOnlyField(field);
                else
                    changed |= DrawField(field);
                continue;
            }

            if ((isDrawRectNode || isDrawGridNode) && field.name == "Color")
            {
                ImGui::Text("%s", field.name.c_str());
                ImGui::SameLine();
                ImGui::PushItemWidth(100.0f);

                char buf[128] = {};
                std::snprintf(buf, sizeof(buf), "%s", field.value.c_str());
                if (ImGui::InputText("##value", buf, sizeof(buf)))
                {
                    field.value = buf;
                    changed = true;
                    drawNodeColorTextChanged = true;
                }
                ImGui::PopItemWidth();
                continue;
            }

            if ((isDrawRectNode || isDrawGridNode) &&
                (field.name == "R" || field.name == "G" || field.name == "B"))
                continue;

            if (isStructCreateNode && field.name == "Schema Fields")
                continue;

            if (isStructDefineNode && field.name == "Fields")
            {
                const int fieldCount = GetArrayItemCountCached(field.value);
                ImGui::TextUnformatted("Fields");
                ImGui::SameLine();
                ImGui::TextDisabled("[%d]", fieldCount);
                continue;
            }

            if (isStructCreateNode && field.name != "Struct Name")
            {
                drawDeferredPinByName(field.name.c_str());
                const bool pinConnected = isInputPinConnected(field.name.c_str());
                if (pinConnected)
                    DrawReadOnlyField(field);
                else
                    changed |= DrawField(field);
                continue;
            }

            changed |= DrawField(field);
        }
        ImGui::PopID();
        ImGui::Spacing();
    }

    if (isSetVariable && !drewDeferredDefaultPin)
        drawDeferredPinByName("Default");

    if (isLoopNode)
    {
        if (!drewDeferredStartPin)
            drawDeferredPinByName("Start");
        if (!drewDeferredCountPin)
            drawDeferredPinByName("Count");
    }

    if (isBinaryDefaultNode)
    {
        if (!drewDeferredAPin)
            drawDeferredPinByName("A");
        if (!drewDeferredBPin)
            drawDeferredPinByName("B");
    }

    if (isUnaryDefaultNode && !drewDeferredAPin)
        drawDeferredPinByName("A");

    if (isDelayNode && !drewDeferredDurationPin)
        drawDeferredPinByName("Duration");

    if (isArrayIndexNode && !drewDeferredIndexPin)
        drawDeferredPinByName("Index");

    if (isDrawRectNode)
    {
        if (!drewDeferredXPin) drawDeferredPinByName("X");
        if (!drewDeferredYPin) drawDeferredPinByName("Y");
        if (!drewDeferredWPin) drawDeferredPinByName("W");
        if (!drewDeferredHPin) drawDeferredPinByName("H");
    }

    if (isDrawGridNode)
    {
        if (!drewDeferredXPin) drawDeferredPinByName("X");
        if (!drewDeferredYPin) drawDeferredPinByName("Y");
        if (!drewDeferredWPin) drawDeferredPinByName("W");
        if (!drewDeferredHPin) drawDeferredPinByName("H");
    }

    if (isDrawRectNode || isDrawGridNode)
    {
        changed |= ClampDrawNodeGeometryFields(n);
        changed |= SyncDrawNodeColorFields(n, drawNodeColorTextChanged);
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

        if (ImGui::SmallButton("- Then"))
        {
            if (n.outPins.size() > 1)
            {
                n.outPins.pop_back();
                changed = true;
            }
        }
    }

    for (const Pin& pin : n.outPins)
        DrawPin(pin, contentWidth, allLinks);

    ed::EndNode();

    const ImVec2 actualNodeSize = ed::GetNodeSize(n.id);
    if (actualNodeSize.x > 0.0f)
        s_nodeWidthCache[nodeKey] = actualNodeSize.x;

    return changed;
}