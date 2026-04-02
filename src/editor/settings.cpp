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
        consolePanelHeight = s.value("consolePanelHeight", consolePanelHeight);
        consolePanelIsMinimized = s.value("consolePanelIsMinimized", consolePanelIsMinimized);

        // Graph canvas colors
        gridBgColorR = s.value("gridBgColorR", gridBgColorR);
        gridBgColorG = s.value("gridBgColorG", gridBgColorG);
        gridBgColorB = s.value("gridBgColorB", gridBgColorB);
        gridBgColorA = s.value("gridBgColorA", gridBgColorA);

        gridLineColorR = s.value("gridLineColorR", gridLineColorR);
        gridLineColorG = s.value("gridLineColorG", gridLineColorG);
        gridLineColorB = s.value("gridLineColorB", gridLineColorB);
        gridLineColorA = s.value("gridLineColorA", gridLineColorA);

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
    s["consolePanelHeight"] = consolePanelHeight;
    s["consolePanelIsMinimized"] = consolePanelIsMinimized;

    // Graph canvas colors
    s["gridBgColorR"] = gridBgColorR;
    s["gridBgColorG"] = gridBgColorG;
    s["gridBgColorB"] = gridBgColorB;
    s["gridBgColorA"] = gridBgColorA;

    s["gridLineColorR"] = gridLineColorR;
    s["gridLineColorG"] = gridLineColorG;
    s["gridLineColorB"] = gridLineColorB;
    s["gridLineColorA"] = gridLineColorA;

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