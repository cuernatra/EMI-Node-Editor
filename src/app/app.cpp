#include "app.h"
#include "panel.h"
#include <imgui-SFML.h>
#include <SFML/Window/Event.hpp>

App::App()
    : m_window(sf::VideoMode(appConstants::windowWidth, appConstants::windowheight), 
    "ImGui + SFML")
    , m_mainEditor{}
    , m_leftPanel{}
{
    m_window.setFramerateLimit(60);
    ImGui::SFML::Init(m_window);
}

void App::run()
{
    while(m_window.isOpen()) 
    {
        processEvents();
        update();
        render();
    }
    ImGui::SFML::Shutdown();
}

void App::processEvents()
{
    sf::Event event;
    while (m_window.pollEvent(event)) 
    {
        ImGui::SFML::ProcessEvent(event);
         switch (event.type) 
         {
            case sf::Event::Closed:
                m_window.close();
                break;
         }
    }
}

void App::update()
{
    ImGui::SFML::Update(m_window, m_deltaClock.restart());
    m_mainEditor.draw();
    m_leftPanel.draw();
}

void App::render()
{
    m_window.clear();
    ImGui::SFML::Render(m_window);
    m_window.display();
}
