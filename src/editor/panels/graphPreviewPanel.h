#pragma once

#include "editor/graph/graphState.h"
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Vector2.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief Separate SFML preview window driven by graph nodes.
 */
class GraphPreviewPanel
{
public:
    void open();
    void close();
    bool isOpen() const;
    void restartPlayback();
    void update(const GraphState& state);
    void setLogSink(std::function<void(const std::string&)> sink);

private:
    struct GridDrawCommand
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        sf::Color color = sf::Color::White;
    };

    struct RectDrawCommand
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        sf::Color color = sf::Color::White;
    };

    static sf::VertexArray BuildGrid(float width, float height, float step, const sf::Color& color, float originX = 0.0f, float originY = 0.0f);
    void renderGraphPreview(const GraphState& state);
    void renderDrawCommands(const GraphState& state);

    std::unique_ptr<sf::RenderWindow> m_window;
    sf::Clock m_playbackClock;
    bool m_hasPlaybackStart = false;

    bool m_hasPendingPick = false;
    sf::Vector2i m_pendingPickPixel{ 0, 0 };

    bool m_hasPickedRect = false;
    float m_pickedRectX = 0.0f;
    float m_pickedRectY = 0.0f;

    std::function<void(const std::string&)> m_logSink;
};
