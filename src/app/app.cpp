#include "app.h"
#include <imgui-SFML.h>
#include <SFML/Window/Event.hpp>
#include <iostream>
#include "constants.h"

App::App()
    : m_window(sf::VideoMode(appConstants::windowWidth, appConstants::windowheight), 
    "EMI editor") 
{
    m_window.setFramerateLimit(60);
    ImGui::SFML::Init(m_window);
    loadFont();
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
    m_ui.draw();
    //m_topPanel.draw();
}

void App::render() 
{
    m_window.clear();

    ImGui::SFML::Render(m_window);
    m_window.display();
}

void App::loadFont()
{
    ImGuiIO& io = ImGui::GetIO();

    io.Fonts->Clear();

    ImFontConfig config;
    config.OversampleH = fontConstants::oversampleH;   // Horizontal oversampling
    config.OversampleV = fontConstants::oversampleV;   // Vertical oversampling 
    config.PixelSnapH  = false;

    io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Regular.ttf", fontConstants::fontSize, &config);

    io.Fonts->Build();
    ImGui::SFML::UpdateFontTexture();
}