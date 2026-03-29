#include "app.h"
#include <imgui-SFML.h>
#include <SFML/Window/Event.hpp>
#include <iostream>
#include "constants.h"
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

#ifdef _WIN32
namespace
{
void setDarkWindowChrome(sf::WindowHandle handle)
{
    if (!handle)
        return;

    const BOOL enableDark = TRUE;
    const HRESULT hr = DwmSetWindowAttribute(
        static_cast<HWND>(handle),
        20,
        &enableDark,
        sizeof(enableDark)
    );

    if (FAILED(hr))
    {
        DwmSetWindowAttribute(
            static_cast<HWND>(handle),
            19,
            &enableDark,
            sizeof(enableDark)
        );
    }
}
}
#endif

App::App()
    : m_window(sf::VideoMode(appConstants::windowWidth, appConstants::windowheight), 
    "EMI editor") 
{
    m_window.setFramerateLimit(60);
#ifdef _WIN32
    setDarkWindowChrome(m_window.getSystemHandle());
#endif
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
    const auto withAlpha = [](ImVec4 color, float alpha)
    {
        color.w = alpha;
        return color;
    };

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
    style.Colors[ImGuiCol_Text]         = colors::textPrimary;
    style.Colors[ImGuiCol_TextDisabled] = colors::textSecondary;

    // Windows & Frames
    style.Colors[ImGuiCol_WindowBg]      = colors::background;
    style.Colors[ImGuiCol_ChildBg]       = colors::surface;
    style.Colors[ImGuiCol_PopupBg]       = colors::elevated;
    style.Colors[ImGuiCol_FrameBg]       = colors::surface;
    style.Colors[ImGuiCol_FrameBgHovered]= colors::elevated;
    style.Colors[ImGuiCol_FrameBgActive] = colors::background;

    // Title Bar
    style.Colors[ImGuiCol_TitleBg]          = colors::surface;
    style.Colors[ImGuiCol_TitleBgCollapsed] = withAlpha(colors::surface, 0.75f);
    style.Colors[ImGuiCol_TitleBgActive]    = colors::accent;

    // Buttons
    style.Colors[ImGuiCol_Button]       = colors::background;
    style.Colors[ImGuiCol_ButtonHovered]= colors::surface;
    style.Colors[ImGuiCol_ButtonActive] = colors::elevated;

    // Headers
    style.Colors[ImGuiCol_Header]       = colors::surface;
    style.Colors[ImGuiCol_HeaderHovered]= colors::elevated;
    style.Colors[ImGuiCol_HeaderActive] = colors::background;

    // Scrollbar
    style.Colors[ImGuiCol_ScrollbarBg]         = colors::transparent;
    style.Colors[ImGuiCol_ScrollbarGrab]        = withAlpha(colors::accent, 0.60f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = withAlpha(colors::accent, 0.80f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = withAlpha(colors::accent, 0.40f);

    // Accents (shared blue highlight)
    style.Colors[ImGuiCol_CheckMark]       = withAlpha(colors::accent, 0.40f);
    style.Colors[ImGuiCol_SliderGrabActive]= withAlpha(colors::accent, 0.40f);
    style.Colors[ImGuiCol_SeparatorHovered]= withAlpha(colors::accent, 0.40f);

    // Misc
    style.Colors[ImGuiCol_MenuBarBg] = colors::elevated;
    style.Colors[ImGuiCol_Border]    = colors::transparent;
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
