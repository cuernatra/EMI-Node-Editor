#include "graphPreviewPanel.h"
#include "../core/graph/types.h"
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Window/Event.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <unordered_set>

namespace
{
constexpr float kPreviewGridStep = 32.0f;
const sf::Color kPreviewBackground(18, 18, 24);
const sf::Color kPreviewEmptyBackground(22, 22, 28);
const sf::Color kPreviewDefaultGridColor(52, 52, 68);
const sf::Color kPreviewGridAreaFill(28, 30, 40, 150);
const sf::Color kPreviewGridAreaOutline(120, 150, 210, 210);

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
    m_window->setFramerateLimit(60);
}

void GraphPreviewPanel::close()
{
    if (m_window)
    {
        m_window->close();
        m_window.reset();
    }
}

bool GraphPreviewPanel::isOpen() const
{
    return m_window && m_window->isOpen();
}

void GraphPreviewPanel::update(const GraphState& state)
{
    if (!m_window || !m_window->isOpen())
        return;

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

    auto findNodeByPin = [&](ed::PinId pinId) -> const VisualNode*
    {
        const Pin* pin = state.FindPin(pinId);
        if (!pin)
            return nullptr;

        return FindNodeById(state, pin->parentNodeId);
    };

    auto resolveFlowTarget = [&](ed::PinId flowOutputPinId) -> const VisualNode*
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
    };

    std::function<double(const VisualNode&, const Pin&)> evalPin;
    std::function<double(const VisualNode&, const char*)> evalNamedInput;

    evalNamedInput = [&](const VisualNode& node, const char* name) -> double
    {
        const Pin* pin = FindInputPin(node, name);
        if (pin)
            return evalPin(node, *pin);

        const NodeField* field = FindField(node, name);
        double out = 0.0;
        if (field && TryParseDouble(field->value, out))
            return out;
        return 0.0;
    };

    evalPin = [&](const VisualNode& owner, const Pin& pin) -> double
    {
        for (const auto& link : links)
        {
            if (!link.alive || link.endPinId != pin.id)
                continue;

            const VisualNode* srcNode = findNodeByPin(link.startPinId);
            if (!srcNode)
                break;

            switch (srcNode->nodeType)
            {
                case NodeType::Constant:
                {
                    const NodeField* valueField = FindField(*srcNode, "Value");
                    double out = 0.0;
                    if (valueField && TryParseDouble(valueField->value, out))
                        return out;
                    return 0.0;
                }

                case NodeType::Operator:
                {
                    const NodeField* opField = FindField(*srcNode, "Op");
                    const std::string op = opField ? opField->value : "+";
                    const double a = evalNamedInput(*srcNode, "A");
                    const double b = evalNamedInput(*srcNode, "B");
                    if (op == "+") return a + b;
                    if (op == "-") return a - b;
                    if (op == "*") return a * b;
                    if (op == "/") return std::abs(b) < 0.000001 ? 0.0 : a / b;
                    return 0.0;
                }

                case NodeType::Comparison:
                {
                    const NodeField* opField = FindField(*srcNode, "Op");
                    const std::string op = opField ? opField->value : "==";
                    const double a = evalNamedInput(*srcNode, "A");
                    const double b = evalNamedInput(*srcNode, "B");
                    if (op == "==") return a == b ? 1.0 : 0.0;
                    if (op == "!=") return a != b ? 1.0 : 0.0;
                    if (op == "<")  return a < b ? 1.0 : 0.0;
                    if (op == "<=") return a <= b ? 1.0 : 0.0;
                    if (op == ">")  return a > b ? 1.0 : 0.0;
                    if (op == ">=") return a >= b ? 1.0 : 0.0;
                    return 0.0;
                }

                case NodeType::Logic:
                {
                    const NodeField* opField = FindField(*srcNode, "Op");
                    const std::string op = opField ? opField->value : "AND";
                    const bool a = evalNamedInput(*srcNode, "A") != 0.0;
                    const bool b = evalNamedInput(*srcNode, "B") != 0.0;
                    if (op == "AND") return (a && b) ? 1.0 : 0.0;
                    if (op == "OR")  return (a || b) ? 1.0 : 0.0;
                    return 0.0;
                }

                case NodeType::Not:
                    return evalNamedInput(*srcNode, "A") == 0.0 ? 1.0 : 0.0;

                default:
                    break;
            }
        }

        const NodeField* field = FindField(owner, pin.name.c_str());
        double out = 0.0;
        if (field && TryParseDouble(field->value, out))
            return out;
        return 0.0;
    };

    struct ScheduledCommand
    {
        long long orderKey = 0;
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
        else if (targetNode->nodeType == NodeType::DrawGrid)
        {
            ScheduledCommand cmd;
            cmd.orderKey = timeCursor * 1000LL + depth;
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
        rect.setOutlineThickness(1.0f);
        rect.setOutlineColor(sf::Color(255, 255, 255, 110));
        m_window->draw(rect);
    }
}
