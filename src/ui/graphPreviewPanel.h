#pragma once

#include "../editor/graphState.h"
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Clock.hpp>
#include <memory>

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
};
