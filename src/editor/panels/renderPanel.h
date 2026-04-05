#pragma once

#include <memory>

/**
 * @brief Separate SFML render window driven by graph nodes.
 */
class RenderPanel
{
public:
    RenderPanel();
    ~RenderPanel();

    // Window lifecycle
    void open();
    void close();
    bool isOpen() const;
    void restartPlayback();
    void update();

    // Drawing (called from EMI VM thread — internally mutex-guarded)
    void drawCell(int x, int y, int r, int g, int b);
    void clearGrid(int w, int h);
    void renderGrid(); // no-op — rendering is frame-driven

    // Input / state query (equivalents of GameFunctions.cpp Input.* / Game.GetPixel)
    int  getCell(int x, int y) const;           // luminance of stored cell (0 = empty)
    int  waitInput();                            // block until key/mouse event; returns code
    bool isKeyDown(int sfKeyCode);               // poll SFML key state (sf::Keyboard::Key as int)
    bool isMouseButtonDown(int button);          // 0=left 1=right 2=middle
    int  getMouseX() const;                      // mouse X in cell coordinates
    int  getMouseY() const;                      // mouse Y in cell coordinates

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};
