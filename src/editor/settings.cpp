#include "editor/settings.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace Settings
{

static constexpr const char* k_path = "settings.json";

void Load()
{
    std::ifstream file(k_path);
    if (!file.is_open())
        return; // No settings file yet — first launch, defaults are used

    try
    {
        nlohmann::json s = nlohmann::json::parse(file);

        // Layout
        leftPanelWidth = s.value("leftPanelWidth", leftPanelWidth);

        // Add new settings here:
        // myNewSetting = s.value("myNewSetting", myNewSetting);
    }
    catch (const nlohmann::json::exception& e)
    {
        std::cerr << "[Settings] Failed to parse settings.json: " << e.what() << "\n";
    }
}

void Save()
{
    nlohmann::json s;

    // Layout
    s["leftPanelWidth"] = leftPanelWidth;

    // Add new settings here:
    // s["myNewSetting"] = myNewSetting;

    std::ofstream file(k_path);
    if (!file.is_open())
    {
        std::cerr << "[Settings] Failed to write settings.json\n";
        return;
    }

    file << s.dump(4);
}

} // namespace Settings