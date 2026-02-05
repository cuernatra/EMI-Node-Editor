// app.h - main application class

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include "mainEditor.h"
#include "topPanel.h"
#include "leftPanel.h"

#ifndef APP_H
#define APP_H

/**
 * @brief Main application class that manages the 
 * render window, events, and editor layer.
 *
 * @author Joel Turkka
 */
class App 
{
public:
    App();
    void run();

private:
    void processEvents();
    void update();
    void render();

    sf::RenderWindow m_window;
    sf::Clock m_deltaClock;
    MainEditor m_mainEditor;
    TopPanel m_topPanel;
    LeftPanel m_leftPanel;
};

#endif