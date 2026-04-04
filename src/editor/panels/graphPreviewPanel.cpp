#include "graphPreviewPanel.h"
#include "ui/theme.h"
#include "core/graph/types.h"
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Window/Event.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace
{
constexpr float kPreviewGridStep = 32.0f;

sf::Color ToSfColor(const ImVec4& c, sf::Uint8 alpha = 255)
{
    auto toByte = [](float v) -> sf::Uint8
    {
        float clamped = std::clamp(v, 0.0f, 1.0f);
        return static_cast<sf::Uint8>(clamped * 255.0f + 0.5f);
    };

    return sf::Color(toByte(c.x), toByte(c.y), toByte(c.z), alpha);
}

const sf::Color kPreviewBackground = ToSfColor(colors::background);
const sf::Color kPreviewEmptyBackground = ToSfColor(colors::surface);
const sf::Color kPreviewDefaultGridColor = ToSfColor(colors::elevated);
const sf::Color kPreviewGridAreaFill = ToSfColor(colors::elevated, 150);
const sf::Color kPreviewGridAreaOutline = ToSfColor(colors::accent, 210);
const sf::Color kPreviewRectOutline = ToSfColor(colors::textPrimary, 110);

void SetPreviewWindowAlwaysOnTop(sf::RenderWindow* window)
{
    if (!window)
        return;

#if defined(_WIN32)
    const sf::WindowHandle handle = window->getSystemHandle();
    if (handle)
    {
        ::SetWindowPos(
            static_cast<HWND>(handle),
            HWND_TOPMOST,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW
        );
    }
#else
    (void)window;
#endif
}

bool TryParseDouble(const std::string& s, double& out)
{
    if (s.empty())
        return false;

    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end && *end == '\0')
    {
        out = v;
        return true;
    }
    return false;
}

const NodeField* FindField(const VisualNode& node, const char* name)
{
    for (const auto& field : node.fields)
        if (field.name == name)
            return &field;
    return nullptr;
}

const Pin* FindInputPin(const VisualNode& node, const char* name)
{
    for (const auto& pin : node.inPins)
        if (pin.name == name)
            return &pin;
    return nullptr;
}

const Pin* FindOutputPin(const VisualNode& node, const char* name)
{
    for (const auto& pin : node.outPins)
        if (pin.name == name)
            return &pin;
    return nullptr;
}

const VisualNode* FindNodeById(const GraphState& state, ed::NodeId nodeId)
{
    for (const auto& node : state.GetNodes())
        if (node.alive && node.id == nodeId)
            return &node;
    return nullptr;
}

struct PreviewEvaluator
{
    struct PreviewValue
    {
        enum class Kind
        {
            Number,
            Array
        };

        Kind kind = Kind::Number;
        double number = 0.0;
        std::vector<PreviewValue> array;

        static PreviewValue Number(double v)
        {
            PreviewValue out;
            out.kind = Kind::Number;
            out.number = v;
            return out;
        }

        static PreviewValue Array(std::vector<PreviewValue> values = {})
        {
            PreviewValue out;
            out.kind = Kind::Array;
            out.array = std::move(values);
            return out;
        }

        bool isArray() const { return kind == Kind::Array; }
    };

    const GraphState& state;
    const std::vector<Link>& links;
    std::unordered_set<uintptr_t> activeEvalPins;
    std::unordered_map<uintptr_t, double> activeLoopIndices;
    std::unordered_map<uintptr_t, PreviewValue> activeForEachElements;
    std::unordered_map<std::string, PreviewValue> variableValues;

    static std::string TrimCopy(const std::string& s)
    {
        size_t a = 0;
        size_t b = s.size();
        while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
        return s.substr(a, b - a);
    }

    static std::vector<std::string> SplitTopLevelArrayItems(const std::string& text)
    {
        std::vector<std::string> out;
        std::string current;
        int bracketDepth = 0;
        bool inQuote = false;
        char quoteChar = '\0';
        bool escape = false;

        auto pushCurrent = [&]() {
            out.push_back(TrimCopy(current));
            current.clear();
        };

        for (char ch : text)
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
                if (ch == '\\') escape = true;
                else if (ch == quoteChar) inQuote = false;
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

            if (ch == ',' && bracketDepth == 0)
            {
                pushCurrent();
                continue;
            }

            current.push_back(ch);
        }

        pushCurrent();
        return out;
    }

    static double ToNumber(const PreviewValue& value)
    {
        if (value.kind == PreviewValue::Kind::Number)
            return value.number;
        return static_cast<double>(value.array.size());
    }

    static PreviewValue ParseLiteral(const std::string& raw)
    {
        const std::string s = TrimCopy(raw);
        if (s.empty())
            return PreviewValue::Number(0.0);

        if (s == "true" || s == "True")
            return PreviewValue::Number(1.0);
        if (s == "false" || s == "False")
            return PreviewValue::Number(0.0);

        if (s.front() == '[' && s.back() == ']')
        {
            const std::string inner = s.substr(1, s.size() - 2);
            const std::vector<std::string> items = SplitTopLevelArrayItems(inner);
            std::vector<PreviewValue> parsed;
            parsed.reserve(items.size());
            for (const std::string& item : items)
            {
                if (item.empty())
                    continue;
                parsed.push_back(ParseLiteral(item));
            }
            return PreviewValue::Array(std::move(parsed));
        }

        double out = 0.0;
        if (TryParseDouble(s, out))
            return PreviewValue::Number(out);

        return PreviewValue::Number(0.0);
    }

    static PreviewValue ParseTypedLiteral(const std::string& typeName, const std::string& valueText)
    {
        if (typeName == "Array")
            return ParseLiteral(valueText);
        if (typeName == "Boolean")
            return PreviewValue::Number((valueText == "true" || valueText == "True" || valueText == "1") ? 1.0 : 0.0);
        if (typeName == "Number")
            return ParseLiteral(valueText);
        return PreviewValue::Number(0.0);
    }

    bool ResolveInputVariableName(const VisualNode& node, const char* inputName, std::string& outName) const
    {
        const Pin* pin = FindInputPin(node, inputName);
        if (!pin)
            return false;

        for (const auto& link : links)
        {
            if (!link.alive || link.endPinId != pin->id)
                continue;

            const VisualNode* srcNode = FindNodeByPin(link.startPinId);
            if (!srcNode || srcNode->nodeType != NodeType::Variable)
                return false;

            const NodeField* variantField = FindField(*srcNode, "Variant");
            const std::string variant = variantField ? variantField->value : "Get";
            if (variant != "Get")
                return false;

            const NodeField* nameField = FindField(*srcNode, "Name");
            if (!nameField || nameField->value.empty())
                return false;

            outName = nameField->value;
            return true;
        }

        return false;
    }

    PreviewValue& EnsureArrayVariable(const std::string& name)
    {
        PreviewValue& v = variableValues[name];
        if (v.kind != PreviewValue::Kind::Array)
        {
            v.kind = PreviewValue::Kind::Array;
            v.number = 0.0;
            v.array.clear();
        }
        return v;
    }

    bool HasIncomingLink(ed::PinId inputPinId) const
    {
        for (const auto& link : links)
            if (link.alive && link.endPinId == inputPinId)
                return true;
        return false;
    }

    const VisualNode* FindNodeByPin(ed::PinId pinId) const
    {
        const Pin* pin = state.FindPin(pinId);
        if (!pin)
            return nullptr;
        return FindNodeById(state, pin->parentNodeId);
    }

    const VisualNode* ResolveFlowTarget(ed::PinId flowOutputPinId) const
    {
        for (const auto& link : links)
        {
            if (!link.alive || link.startPinId != flowOutputPinId)
                continue;

            const Pin* endPin = state.FindPin(link.endPinId);
            if (!endPin || endPin->type != PinType::Flow)
                continue;

            return FindNodeById(state, endPin->parentNodeId);
        }

        return nullptr;
    }

    PreviewValue EvalValueNamedInput(const VisualNode& node, const char* name)
    {
        const Pin* pin = FindInputPin(node, name);
        if (pin)
            return EvalValuePin(node, *pin);

        const NodeField* field = FindField(node, name);
        if (field)
            return ParseLiteral(field->value);
        return PreviewValue::Number(0.0);
    }

    double EvalNamedInput(const VisualNode& node, const char* name)
    {
        return ToNumber(EvalValueNamedInput(node, name));
    }

    PreviewValue EvalValuePin(const VisualNode& owner, const Pin& pin)
    {
        const uintptr_t evalKey = static_cast<uintptr_t>(pin.id.Get());
        if (!activeEvalPins.insert(evalKey).second)
            return PreviewValue::Number(0.0);

        struct ScopedErase
        {
            std::unordered_set<uintptr_t>& set;
            uintptr_t key;
            ~ScopedErase() { set.erase(key); }
        } guard{ activeEvalPins, evalKey };

        for (const auto& link : links)
        {
            if (!link.alive || link.endPinId != pin.id)
                continue;

            const VisualNode* srcNode = FindNodeByPin(link.startPinId);
            if (!srcNode)
                break;

            switch (srcNode->nodeType)
            {
                case NodeType::Loop:
                {
                    const Pin* srcPin = state.FindPin(link.startPinId);
                    if (srcPin && srcPin->name == "Index")
                    {
                        const uintptr_t loopId = static_cast<uintptr_t>(srcNode->id.Get());
                        const auto it = activeLoopIndices.find(loopId);
                        if (it != activeLoopIndices.end())
                            return PreviewValue::Number(it->second);
                    }
                    return PreviewValue::Number(0.0);
                }

                case NodeType::ForEach:
                {
                    const Pin* srcPin = state.FindPin(link.startPinId);
                    if (srcPin && srcPin->name == "Element")
                    {
                        const uintptr_t foreachId = static_cast<uintptr_t>(srcNode->id.Get());
                        const auto it = activeForEachElements.find(foreachId);
                        if (it != activeForEachElements.end())
                            return it->second;
                    }
                    return PreviewValue::Number(0.0);
                }

                case NodeType::Constant:
                {
                    const NodeField* typeField = FindField(*srcNode, "Type");
                    const NodeField* valueField = FindField(*srcNode, "Value");
                    return ParseTypedLiteral(typeField ? typeField->value : "Number", valueField ? valueField->value : "0");
                }

                case NodeType::Operator:
                {
                    const NodeField* opField = FindField(*srcNode, "Op");
                    const std::string op = opField ? opField->value : "+";
                    const double a = EvalNamedInput(*srcNode, "A");
                    const double b = EvalNamedInput(*srcNode, "B");
                    if (op == "+") return PreviewValue::Number(a + b);
                    if (op == "-") return PreviewValue::Number(a - b);
                    if (op == "*") return PreviewValue::Number(a * b);
                    if (op == "/") return PreviewValue::Number(std::abs(b) < 0.000001 ? 0.0 : a / b);
                    return PreviewValue::Number(0.0);
                }

                case NodeType::Comparison:
                {
                    const NodeField* opField = FindField(*srcNode, "Op");
                    const std::string op = opField ? opField->value : "==";
                    const double a = EvalNamedInput(*srcNode, "A");
                    const double b = EvalNamedInput(*srcNode, "B");
                    if (op == "==") return PreviewValue::Number(a == b ? 1.0 : 0.0);
                    if (op == "!=") return PreviewValue::Number(a != b ? 1.0 : 0.0);
                    if (op == "<")  return PreviewValue::Number(a < b ? 1.0 : 0.0);
                    if (op == "<=") return PreviewValue::Number(a <= b ? 1.0 : 0.0);
                    if (op == ">")  return PreviewValue::Number(a > b ? 1.0 : 0.0);
                    if (op == ">=") return PreviewValue::Number(a >= b ? 1.0 : 0.0);
                    return PreviewValue::Number(0.0);
                }

                case NodeType::Logic:
                {
                    const NodeField* opField = FindField(*srcNode, "Op");
                    const std::string op = opField ? opField->value : "AND";
                    const bool a = EvalNamedInput(*srcNode, "A") != 0.0;
                    const bool b = EvalNamedInput(*srcNode, "B") != 0.0;
                    if (op == "AND") return PreviewValue::Number((a && b) ? 1.0 : 0.0);
                    if (op == "OR")  return PreviewValue::Number((a || b) ? 1.0 : 0.0);
                    return PreviewValue::Number(0.0);
                }

                case NodeType::Not:
                    return PreviewValue::Number(EvalNamedInput(*srcNode, "A") == 0.0 ? 1.0 : 0.0);

                case NodeType::Variable:
                {
                    const NodeField* variantField = FindField(*srcNode, "Variant");
                    const std::string variant = variantField ? variantField->value : "Get";
                    if (variant != "Get")
                        return PreviewValue::Number(0.0);

                    const NodeField* nameField = FindField(*srcNode, "Name");
                    if (!nameField)
                        return PreviewValue::Number(0.0);

                    const auto it = variableValues.find(nameField->value);
                    if (it != variableValues.end())
                        return it->second;

                    const NodeField* typeField = FindField(*srcNode, "Type");
                    const NodeField* defaultField = FindField(*srcNode, "Default");
                    return ParseTypedLiteral(typeField ? typeField->value : "Number", defaultField ? defaultField->value : "0");
                }

                case NodeType::ArrayGetAt:
                {
                    const PreviewValue arrayValue = EvalValueNamedInput(*srcNode, "Array");
                    const int index = static_cast<int>(std::llround(EvalNamedInput(*srcNode, "Index")));
                    if (!arrayValue.isArray() || index < 0 || index >= static_cast<int>(arrayValue.array.size()))
                        return PreviewValue::Number(0.0);
                    return arrayValue.array[static_cast<size_t>(index)];
                }

                case NodeType::ArrayLength:
                {
                    const PreviewValue arrayValue = EvalValueNamedInput(*srcNode, "Array");
                    return PreviewValue::Number(arrayValue.isArray() ? static_cast<double>(arrayValue.array.size()) : 0.0);
                }

                case NodeType::StructCreate:
                {
                    std::vector<PreviewValue> fields;
                    fields.reserve(srcNode->inPins.size());
                    for (const Pin& inPin : srcNode->inPins)
                    {
                        if (inPin.type == PinType::Flow)
                            continue;
                        if (inPin.name == "Struct" || inPin.name == "Schema" ||
                            inPin.name == "Struct Name" || inPin.name == "Schema Fields")
                            continue;
                        fields.push_back(EvalValuePin(*srcNode, inPin));
                    }
                    return PreviewValue::Array(std::move(fields));
                }

                default:
                    break;
            }
        }

        const NodeField* field = FindField(owner, pin.name.c_str());
        if (field)
            return ParseLiteral(field->value);
        return PreviewValue::Number(0.0);
    }
};
}

sf::VertexArray GraphPreviewPanel::BuildGrid(float width, float height, float step, const sf::Color& color, float originX, float originY)
{
    sf::VertexArray grid(sf::Lines);

    if (step <= 0.0f || width <= 0.0f || height <= 0.0f)
        return grid;

    for (float x = 0.0f; x <= width; x += step)
    {
        grid.append(sf::Vertex(sf::Vector2f(originX + x, originY), color));
        grid.append(sf::Vertex(sf::Vector2f(originX + x, originY + height), color));
    }

    for (float y = 0.0f; y <= height; y += step)
    {
        grid.append(sf::Vertex(sf::Vector2f(originX, originY + y), color));
        grid.append(sf::Vertex(sf::Vector2f(originX + width, originY + y), color));
    }

    return grid;
}

void GraphPreviewPanel::open()
{
    if (m_window && m_window->isOpen())
        return;

    m_window = std::make_unique<sf::RenderWindow>(
        sf::VideoMode(900, 700),
        "EMI Visual Preview",
        sf::Style::Titlebar | sf::Style::Close
    );
    SetPreviewWindowAlwaysOnTop(m_window.get());
    m_window->setFramerateLimit(60);
    m_playbackClock.restart();
    m_hasPlaybackStart = true;
}

void GraphPreviewPanel::close()
{
    if (m_window)
    {
        m_window->close();
        m_window.reset();
    }

    m_hasPlaybackStart = false;
}

bool GraphPreviewPanel::isOpen() const
{
    return m_window && m_window->isOpen();
}

void GraphPreviewPanel::restartPlayback()
{
    m_playbackClock.restart();
    m_hasPlaybackStart = true;
}

void GraphPreviewPanel::update(const GraphState& state)
{
    if (!m_window || !m_window->isOpen())
        return;

    if (!m_hasPlaybackStart)
    {
        m_playbackClock.restart();
        m_hasPlaybackStart = true;
    }

    sf::Event event{};
    while (m_window->pollEvent(event))
    {
        if (event.type == sf::Event::Closed)
        {
            close();
            return;
        }
    }

    renderGraphPreview(state);
    m_window->display();
}

void GraphPreviewPanel::renderGraphPreview(const GraphState& state)
{
    if (!m_window)
        return;

    const float width = static_cast<float>(m_window->getSize().x);
    const float height = static_cast<float>(m_window->getSize().y);
    m_window->clear(kPreviewBackground);

    sf::RectangleShape background(sf::Vector2f(width, height));
    background.setFillColor(kPreviewEmptyBackground);
    m_window->draw(background);
    m_window->draw(BuildGrid(width, height, kPreviewGridStep, kPreviewDefaultGridColor, 0.0f, 0.0f));
    renderDrawCommands(state);
}

void GraphPreviewPanel::renderDrawCommands(const GraphState& state)
{
    if (!m_window)
        return;

    const auto& nodes = state.GetNodes();
    const auto& links = state.GetLinks();
    PreviewEvaluator evaluator{ state, links };
    auto evalNamedInput = [&](const VisualNode& node, const char* name) -> double
    {
        return evaluator.EvalNamedInput(node, name);
    };

    auto resolveFlowTarget = [&](ed::PinId flowOutputPinId) -> const VisualNode*
    {
        return evaluator.ResolveFlowTarget(flowOutputPinId);
    };

    struct ScheduledCommand
    {
        long long orderKey = 0;
        long long timeMs = 0;
        bool isGrid = false;
        GridDrawCommand grid;
        RectDrawCommand rect;
    };

    struct ActiveGridContext
    {
        bool valid = false;
        float originX = 0.0f;
        float originY = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    std::vector<ScheduledCommand> commands;
    commands.reserve(nodes.size());

    const VisualNode* startNode = nullptr;
    for (const auto& node : nodes)
    {
        if (node.alive && node.nodeType == NodeType::Start)
        {
            startNode = &node;
            break;
        }
    }

    std::unordered_set<uintptr_t> activeFlowOutputs;
    std::function<void(ed::PinId, long long&, int, ActiveGridContext)> appendFlowChain;

    appendFlowChain = [&](ed::PinId flowOutputPinId, long long& timeCursor, int depth, ActiveGridContext activeGrid)
    {
        const uintptr_t flowKey = static_cast<uintptr_t>(flowOutputPinId.Get());
        if (!activeFlowOutputs.insert(flowKey).second)
            return;

        struct ScopedErase
        {
            std::unordered_set<uintptr_t>& set;
            uintptr_t key;
            ~ScopedErase() { set.erase(key); }
        } guard{ activeFlowOutputs, flowKey };

        const VisualNode* targetNode = resolveFlowTarget(flowOutputPinId);
        if (!targetNode)
            return;

        if (targetNode->nodeType == NodeType::Delay)
        {
            timeCursor += static_cast<long long>(std::llround(std::max(0.0, evalNamedInput(*targetNode, "Duration"))));
        }
        else if (targetNode->nodeType == NodeType::Loop)
        {
            const int start = static_cast<int>(std::llround(evalNamedInput(*targetNode, "Start")));
            const int count = std::max(0, static_cast<int>(std::llround(evalNamedInput(*targetNode, "Count"))));

            const Pin* bodyOut = FindOutputPin(*targetNode, "Body");
            const Pin* completedOut = FindOutputPin(*targetNode, "Completed");

            const uintptr_t loopId = static_cast<uintptr_t>(targetNode->id.Get());
            long long loopCursor = timeCursor;

            if (bodyOut)
            {
                for (int i = 0; i < count; ++i)
                {
                    evaluator.activeLoopIndices[loopId] = static_cast<double>(start + i);
                    appendFlowChain(bodyOut->id, loopCursor, depth + 1, activeGrid);
                }
            }

            evaluator.activeLoopIndices.erase(loopId);

            timeCursor = loopCursor;
            if (completedOut)
                appendFlowChain(completedOut->id, timeCursor, depth + 1, activeGrid);
            return;
        }
        else if (targetNode->nodeType == NodeType::ForEach)
        {
            const Pin* bodyOut = FindOutputPin(*targetNode, "Body");
            const Pin* completedOut = FindOutputPin(*targetNode, "Completed");
            const auto arrayValue = evaluator.EvalValueNamedInput(*targetNode, "Array");

            if (bodyOut && arrayValue.kind == PreviewEvaluator::PreviewValue::Kind::Array)
            {
                const uintptr_t foreachId = static_cast<uintptr_t>(targetNode->id.Get());
                long long foreachCursor = timeCursor;
                for (const auto& elem : arrayValue.array)
                {
                    evaluator.activeForEachElements[foreachId] = elem;
                    appendFlowChain(bodyOut->id, foreachCursor, depth + 1, activeGrid);
                }
                evaluator.activeForEachElements.erase(foreachId);
                timeCursor = foreachCursor;
            }

            if (completedOut)
                appendFlowChain(completedOut->id, timeCursor, depth + 1, activeGrid);
            return;
        }
        else if (targetNode->nodeType == NodeType::Sequence)
        {
            int branchIndex = 0;
            for (const Pin& outPin : targetNode->outPins)
            {
                if (outPin.type != PinType::Flow)
                    continue;

                long long branchCursor = timeCursor + static_cast<long long>(branchIndex) * 1000000LL;
                appendFlowChain(outPin.id, branchCursor, depth + 1, activeGrid);
                ++branchIndex;
            }
            return;
        }
        else if (targetNode->nodeType == NodeType::Branch)
        {
            const bool condition = evalNamedInput(*targetNode, "Condition") != 0.0;
            const Pin* selectedOut = condition
                ? FindOutputPin(*targetNode, "True")
                : FindOutputPin(*targetNode, "False");

            if (selectedOut)
                appendFlowChain(selectedOut->id, timeCursor, depth + 1, activeGrid);

            return;
        }
        else if (targetNode->nodeType == NodeType::Variable)
        {
            const NodeField* variantField = FindField(*targetNode, "Variant");
            const std::string variant = variantField ? variantField->value : "Set";
            if (variant != "Get")
            {
                const NodeField* nameField = FindField(*targetNode, "Name");
                if (nameField && !nameField->value.empty())
                    evaluator.variableValues[nameField->value] = evaluator.EvalValueNamedInput(*targetNode, "Default");
            }
        }
        else if (targetNode->nodeType == NodeType::ArrayAddAt)
        {
            std::string arrayVarName;
            if (evaluator.ResolveInputVariableName(*targetNode, "Array", arrayVarName))
            {
                auto& arrayVar = evaluator.EnsureArrayVariable(arrayVarName);
                int insertIndex = static_cast<int>(std::llround(evalNamedInput(*targetNode, "Index")));
                insertIndex = std::clamp(insertIndex, 0, static_cast<int>(arrayVar.array.size()));

                const Pin* valuePin = FindInputPin(*targetNode, "Value");
                PreviewEvaluator::PreviewValue value;
                if (valuePin && evaluator.HasIncomingLink(valuePin->id))
                {
                    value = evaluator.EvalValueNamedInput(*targetNode, "Value");
                }
                else
                {
                    const NodeField* addType = FindField(*targetNode, "Add Type");
                    const NodeField* addValue = FindField(*targetNode, "Add Value");
                    value = PreviewEvaluator::ParseTypedLiteral(addType ? addType->value : "Number", addValue ? addValue->value : "0");
                }

                arrayVar.array.insert(arrayVar.array.begin() + insertIndex, value);
            }
        }
        else if (targetNode->nodeType == NodeType::ArrayReplaceAt)
        {
            std::string arrayVarName;
            if (evaluator.ResolveInputVariableName(*targetNode, "Array", arrayVarName))
            {
                auto& arrayVar = evaluator.EnsureArrayVariable(arrayVarName);
                int index = static_cast<int>(std::llround(evalNamedInput(*targetNode, "Index")));

                const Pin* valuePin = FindInputPin(*targetNode, "Value");
                PreviewEvaluator::PreviewValue value;
                if (valuePin && evaluator.HasIncomingLink(valuePin->id))
                {
                    value = evaluator.EvalValueNamedInput(*targetNode, "Value");
                }
                else
                {
                    const NodeField* replaceType = FindField(*targetNode, "Replace Type");
                    const NodeField* replaceValue = FindField(*targetNode, "Replace Value");
                    value = PreviewEvaluator::ParseTypedLiteral(replaceType ? replaceType->value : "Number", replaceValue ? replaceValue->value : "0");
                }

                if (index >= 0 && index < static_cast<int>(arrayVar.array.size()))
                {
                    arrayVar.array[static_cast<size_t>(index)] = value;
                }
                else if (index == static_cast<int>(arrayVar.array.size()))
                {
                    arrayVar.array.push_back(value);
                }
            }
        }
        else if (targetNode->nodeType == NodeType::ArrayRemoveAt)
        {
            std::string arrayVarName;
            if (evaluator.ResolveInputVariableName(*targetNode, "Array", arrayVarName))
            {
                auto& arrayVar = evaluator.EnsureArrayVariable(arrayVarName);
                const int index = static_cast<int>(std::llround(evalNamedInput(*targetNode, "Index")));
                if (index >= 0 && index < static_cast<int>(arrayVar.array.size()))
                    arrayVar.array.erase(arrayVar.array.begin() + index);
            }
        }
        else if (targetNode->nodeType == NodeType::DrawGrid)
        {
            ScheduledCommand cmd;
            cmd.orderKey = timeCursor * 1000LL + depth;
            cmd.timeMs = timeCursor;
            cmd.isGrid = true;
            cmd.grid.x = static_cast<float>(evalNamedInput(*targetNode, "X"));
            cmd.grid.y = static_cast<float>(evalNamedInput(*targetNode, "Y"));
            cmd.grid.width = std::max(1.0f, static_cast<float>(evalNamedInput(*targetNode, "W")));
            cmd.grid.height = std::max(1.0f, static_cast<float>(evalNamedInput(*targetNode, "H")));
            cmd.grid.color = sf::Color(
                static_cast<sf::Uint8>(std::clamp(evalNamedInput(*targetNode, "R"), 0.0, 255.0)),
                static_cast<sf::Uint8>(std::clamp(evalNamedInput(*targetNode, "G"), 0.0, 255.0)),
                static_cast<sf::Uint8>(std::clamp(evalNamedInput(*targetNode, "B"), 0.0, 255.0)),
                255
            );
            commands.push_back(cmd);

            activeGrid.valid = true;
            activeGrid.originX = cmd.grid.x;
            activeGrid.originY = cmd.grid.y;
            activeGrid.width = cmd.grid.width;
            activeGrid.height = cmd.grid.height;
        }
        else if (targetNode->nodeType == NodeType::DrawRect)
        {
            ScheduledCommand cmd;
            cmd.orderKey = timeCursor * 1000LL + depth;
            cmd.timeMs = timeCursor;
            cmd.isGrid = false;
            float localX = static_cast<float>(evalNamedInput(*targetNode, "X"));
            float localY = static_cast<float>(evalNamedInput(*targetNode, "Y"));
            float rectWidth = std::max(0.0f, static_cast<float>(evalNamedInput(*targetNode, "W")));
            float rectHeight = std::max(0.0f, static_cast<float>(evalNamedInput(*targetNode, "H")));

            if (activeGrid.valid)
            {
                localX = std::clamp(localX, 0.0f, activeGrid.width);
                localY = std::clamp(localY, 0.0f, activeGrid.height);
                rectWidth = std::clamp(rectWidth, 0.0f, std::max(0.0f, activeGrid.width - localX));
                rectHeight = std::clamp(rectHeight, 0.0f, std::max(0.0f, activeGrid.height - localY));
                cmd.rect.x = activeGrid.originX + localX;
                cmd.rect.y = activeGrid.originY + localY;
            }
            else
            {
                cmd.rect.x = std::max(0.0f, localX);
                cmd.rect.y = std::max(0.0f, localY);
            }

            cmd.rect.width = rectWidth;
            cmd.rect.height = rectHeight;
            cmd.rect.color = sf::Color(
                static_cast<sf::Uint8>(std::clamp(evalNamedInput(*targetNode, "R"), 0.0, 255.0)),
                static_cast<sf::Uint8>(std::clamp(evalNamedInput(*targetNode, "G"), 0.0, 255.0)),
                static_cast<sf::Uint8>(std::clamp(evalNamedInput(*targetNode, "B"), 0.0, 255.0)),
                220
            );
            commands.push_back(cmd);
        }

        const Pin* outFlow = FindOutputPin(*targetNode, "Out");
        if (!outFlow)
        {
            for (const auto& pin : targetNode->outPins)
            {
                if (pin.type == PinType::Flow)
                {
                    outFlow = &pin;
                    break;
                }
            }
        }

        if (outFlow)
            appendFlowChain(outFlow->id, timeCursor, depth + 1, activeGrid);
    };

    bool usedFlowExecution = false;
    if (startNode)
    {
        const Pin* startOut = FindOutputPin(*startNode, "Exec");
        if (!startOut)
        {
            for (const auto& pin : startNode->outPins)
            {
                if (pin.type == PinType::Flow)
                {
                    startOut = &pin;
                    break;
                }
            }
        }

        if (startOut)
        {
            long long timeCursor = 0;
            appendFlowChain(startOut->id, timeCursor, 0, ActiveGridContext{});
            usedFlowExecution = true;
        }
    }

    if (!usedFlowExecution)
    {
        for (const auto& node : nodes)
        {
            if (!node.alive || node.nodeType != NodeType::DrawRect)
                continue;

            ScheduledCommand cmd;
            cmd.orderKey = static_cast<long long>(commands.size());
            cmd.timeMs = 0;
            cmd.isGrid = false;
            cmd.rect.x = static_cast<float>(evalNamedInput(node, "X"));
            cmd.rect.y = static_cast<float>(evalNamedInput(node, "Y"));
            cmd.rect.width = std::max(2.0f, static_cast<float>(evalNamedInput(node, "W")));
            cmd.rect.height = std::max(2.0f, static_cast<float>(evalNamedInput(node, "H")));
            cmd.rect.color = sf::Color(
                static_cast<sf::Uint8>(std::clamp(evalNamedInput(node, "R"), 0.0, 255.0)),
                static_cast<sf::Uint8>(std::clamp(evalNamedInput(node, "G"), 0.0, 255.0)),
                static_cast<sf::Uint8>(std::clamp(evalNamedInput(node, "B"), 0.0, 255.0)),
                220
            );
            commands.push_back(cmd);
        }
    }

    std::stable_sort(commands.begin(), commands.end(), [](const ScheduledCommand& a, const ScheduledCommand& b)
    {
        return a.orderKey < b.orderKey;
    });

    // Normalize timeline so the first drawable command appears immediately
    // when preview playback starts (no initial blank wait).
    long long firstCommandTimeMs = std::numeric_limits<long long>::max();
    for (const auto& cmd : commands)
        firstCommandTimeMs = std::min(firstCommandTimeMs, cmd.timeMs);

    if (firstCommandTimeMs != std::numeric_limits<long long>::max() && firstCommandTimeMs > 0)
    {
        for (auto& cmd : commands)
            cmd.timeMs = std::max(0LL, cmd.timeMs - firstCommandTimeMs);
    }

    const long long elapsedMs = m_hasPlaybackStart
        ? static_cast<long long>(m_playbackClock.getElapsedTime().asMilliseconds())
        : std::numeric_limits<long long>::max();

    struct ViewTransform
    {
        float scale = 1.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
    } transform;

    bool hasSceneContent = false;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    auto includeBounds = [&](float x, float y, float w, float h)
    {
        if (w <= 0.0f || h <= 0.0f)
            return;

        if (!hasSceneContent)
        {
            minX = x;
            minY = y;
            maxX = x + w;
            maxY = y + h;
            hasSceneContent = true;
            return;
        }

        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x + w);
        maxY = std::max(maxY, y + h);
    };

    for (const auto& cmd : commands)
    {
        if (cmd.timeMs > elapsedMs)
            continue;

        if (cmd.isGrid)
            includeBounds(cmd.grid.x, cmd.grid.y, cmd.grid.width, cmd.grid.height);
        else
            includeBounds(cmd.rect.x, cmd.rect.y, cmd.rect.width, cmd.rect.height);
    }

    if (hasSceneContent)
    {
        const float viewportWidth = static_cast<float>(m_window->getSize().x);
        const float viewportHeight = static_cast<float>(m_window->getSize().y);
        const float margin = 24.0f;
        const float contentWidth = std::max(1.0f, maxX - minX);
        const float contentHeight = std::max(1.0f, maxY - minY);
        const float fitWidth = std::max(1.0f, viewportWidth - margin * 2.0f);
        const float fitHeight = std::max(1.0f, viewportHeight - margin * 2.0f);

        transform.scale = std::min(fitWidth / contentWidth, fitHeight / contentHeight);
        transform.offsetX = margin + (fitWidth - contentWidth * transform.scale) * 0.5f - minX * transform.scale;
        transform.offsetY = margin + (fitHeight - contentHeight * transform.scale) * 0.5f - minY * transform.scale;
    }

    for (const auto& cmd : commands)
    {
        if (cmd.timeMs > elapsedMs)
            continue;

        auto tx = [&](float x) { return x * transform.scale + transform.offsetX; };
        auto ty = [&](float y) { return y * transform.scale + transform.offsetY; };

        if (cmd.isGrid)
        {
            sf::RectangleShape gridArea(sf::Vector2f(
                cmd.grid.width * transform.scale,
                cmd.grid.height * transform.scale
            ));
            gridArea.setPosition(tx(cmd.grid.x), ty(cmd.grid.y));
            gridArea.setFillColor(kPreviewGridAreaFill);
            gridArea.setOutlineThickness(std::max(1.0f, transform.scale));
            gridArea.setOutlineColor(sf::Color(
                cmd.grid.color.r,
                cmd.grid.color.g,
                cmd.grid.color.b,
                kPreviewGridAreaOutline.a
            ));
            m_window->draw(gridArea);
            continue;
        }

        sf::RectangleShape rect(sf::Vector2f(
            std::max(1.0f, cmd.rect.width * transform.scale),
            std::max(1.0f, cmd.rect.height * transform.scale)
        ));
        rect.setPosition(tx(cmd.rect.x), ty(cmd.rect.y));
        rect.setFillColor(cmd.rect.color);
        // Keep outlines inside the rectangle so adjacent unit-sized rects
        // (e.g. X=0 and X=1 with W=1) do not visually overlap.
        rect.setOutlineThickness(-1.0f);
        rect.setOutlineColor(kPreviewRectOutline);
        m_window->draw(rect);
    }
}
