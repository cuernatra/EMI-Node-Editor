#include "app.h"
#include <imgui-SFML.h>
#include <SFML/Window/Event.hpp>

App::App()
    : window(sf::VideoMode(1280, 720), "ImGui + SFML") {
    window.setFramerateLimit(60);
    ImGui::SFML::Init(window);
}

void App::run() {
    while (window.isOpen()) {
        processEvents();
        update();
        render();
    }
    ImGui::SFML::Shutdown();
}

void App::processEvents() {
    sf::Event event;
    while (window.pollEvent(event)) {
        ImGui::SFML::ProcessEvent(event);
         switch (event.type) {
            case sf::Event::Closed:
                window.close();
                break;
         }
    }
}

void App::update() {
    ImGui::SFML::Update(window, deltaClock.restart());
    editor.draw();
}

void App::render() {
    window.clear();
    ImGui::SFML::Render(window);
    window.display();
}
