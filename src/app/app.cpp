#include "app.h"
#include <imgui-SFML.h>
#include <SFML/Window/Event.hpp>
#include <iostream>
#include "constants.h"
#include <filesystem>

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

void App::applyImGuiStyle()
{
    auto& style = ImGui::GetStyle();

    // --- Layout & Spacing ---
    style.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
    style.WindowPadding     = ImVec2(10.f, 10.f);
    style.WindowRounding    = 5.0f;
    style.FramePadding      = ImVec2(5.f, 1.f);
    style.FrameRounding     = 4.0f;
    style.ItemSpacing       = ImVec2(5.f, 5.f);
    style.ItemInnerSpacing  = ImVec2(8.f, 6.f);
    style.IndentSpacing     = 25.0f;
    style.ScrollbarSize     = 4.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabMinSize       = 10.0f;
    style.GrabRounding      = 4.0f;
    style.Alpha             = 1.0f;

    // Text
    style.Colors[ImGuiCol_Text]         = ImVec4(1.86f, 1.93f, 1.89f, 0.78f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.86f, 0.93f, 0.89f, 0.78f);

    // Windows & Frames
    style.Colors[ImGuiCol_WindowBg]      = ImColor(20, 20, 20, 255);
    style.Colors[ImGuiCol_ChildBg]       = ImColor(15, 15, 15, 255);
    style.Colors[ImGuiCol_PopupBg]       = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    style.Colors[ImGuiCol_FrameBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.09f);
    style.Colors[ImGuiCol_FrameBgHovered]= ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.04f, 0.04f, 0.04f, 0.88f);

    // Title Bar
    style.Colors[ImGuiCol_TitleBg]          = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.20f, 0.22f, 0.27f, 0.75f);
    style.Colors[ImGuiCol_TitleBgActive]    = ImVec4(0.15f, 0.60f, 0.78f, 1.00f);

    // Buttons
    style.Colors[ImGuiCol_Button]       = ImVec4(0.10f,  0.10f,  0.10f,  1.00f);
    style.Colors[ImGuiCol_ButtonHovered]= ImVec4(0.15f,  0.15f,  0.15f,  1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.125f, 0.125f, 0.125f, 1.00f);

    // Headers
    style.Colors[ImGuiCol_Header]       = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered]= ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);

    // Scrollbar
    style.Colors[ImGuiCol_ScrollbarBg]         = ImColor(0, 0, 0, 0);
    style.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.26f, 0.59f, 0.98f, 0.60f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);

    // Accents (shared blue highlight)
    style.Colors[ImGuiCol_CheckMark]       = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    style.Colors[ImGuiCol_SliderGrabActive]= ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    style.Colors[ImGuiCol_SeparatorHovered]= ImVec4(0.26f, 0.59f, 0.98f, 0.40f);

    // Misc
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_Border]    = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
}

void App::loadFont()
{
    applyImGuiStyle();

    ImGuiIO& io = ImGui::GetIO();

    // fonts
    ImFontConfig config;
    config.OversampleH = fontConstants::oversampleH;   // Horizontal oversampling
    config.OversampleV = fontConstants::oversampleV;   // Vertical oversampling 
    config.PixelSnapH  = false;

    io.Fonts->Clear();
    #ifdef _WIN32
    std::filesystem::path fontPath = std::filesystem::path(_pgmptr).parent_path() / "assets/fonts/Roboto-Regular.ttf";
    if (std::filesystem::exists(fontPath))
    {
        io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), fontConstants::fontSize, &config);
    }
    else 
    {
        std::cerr << "[ERROR] Font file not found at " << fontPath.string() << std::endl;
        io.Fonts->AddFontDefault(&config);
    }
    #else    
    io.Fonts->AddFontFromFileTTF("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", fontConstants::fontSize, &config);
    #endif

    io.Fonts->Build();
    ImGui::SFML::UpdateFontTexture();
}
