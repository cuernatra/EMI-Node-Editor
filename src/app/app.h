/**
 * @file app.h
 * @brief Main application class definition
 * 
 * Defines the core App class that manages the SFML window, event loop,
 * and UI rendering for the visual programming tool.
 * 
 * @author Joel Turkka
 */

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include "ui.h"

#ifndef APP_H
#define APP_H

/**
 * @brief Main application class that manages the render window, events, and editor layer
 * 
 * Orchestrates the application lifecycle: creates and manages the SFML window,
 * processes events, updates the UI state, and renders frames at 60fps.
 * Integrates ImGui-SFML for immediate mode GUI rendering.
 * 
 * @author Joel Turkka
 */
class App 
{
public:
    /**
     * @brief Initialize the application and create the window
     */
    App();
    
    /**
     * @brief Run the main application loop until window closes
     */
    void run();

private:
    /**
     * @brief Process SFML and ImGui events for the current frame
     */
    void processEvents();
    
    /**
     * @brief Update application state and UI for the current frame
     */
    void update();
    
    /**
     * @brief Render the frame to the window
     */
    void render();

    /**
    * @brief Load font
    * Loads a custom TTF font for use in ImGui. Adjusts oversampling for better quality.
    */
    void loadFont();

    /**
    * @brief Apply custom ImGui style settings
    */
    void applyImGuiStyle();

    /// SFML window for rendering
    sf::RenderWindow m_window;
    
    /// Clock for tracking delta time between frames
    sf::Clock m_deltaClock;
    
    /// Main UI compositor that manages all editor panels
    Ui m_ui;
};

#endif