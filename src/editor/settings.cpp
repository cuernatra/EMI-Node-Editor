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

        nodeHeaderEventColorR = s.value("nodeHeaderEventColorR", nodeHeaderEventColorR);
        nodeHeaderEventColorG = s.value("nodeHeaderEventColorG", nodeHeaderEventColorG);
        nodeHeaderEventColorB = s.value("nodeHeaderEventColorB", nodeHeaderEventColorB);
        nodeHeaderEventColorA = s.value("nodeHeaderEventColorA", nodeHeaderEventColorA);

        nodeHeaderDataColorR = s.value("nodeHeaderDataColorR", nodeHeaderDataColorR);
        nodeHeaderDataColorG = s.value("nodeHeaderDataColorG", nodeHeaderDataColorG);
        nodeHeaderDataColorB = s.value("nodeHeaderDataColorB", nodeHeaderDataColorB);
        nodeHeaderDataColorA = s.value("nodeHeaderDataColorA", nodeHeaderDataColorA);

        nodeHeaderStructColorR = s.value("nodeHeaderStructColorR", nodeHeaderStructColorR);
        nodeHeaderStructColorG = s.value("nodeHeaderStructColorG", nodeHeaderStructColorG);
        nodeHeaderStructColorB = s.value("nodeHeaderStructColorB", nodeHeaderStructColorB);
        nodeHeaderStructColorA = s.value("nodeHeaderStructColorA", nodeHeaderStructColorA);

        nodeHeaderLogicColorR = s.value("nodeHeaderLogicColorR", nodeHeaderLogicColorR);
        nodeHeaderLogicColorG = s.value("nodeHeaderLogicColorG", nodeHeaderLogicColorG);
        nodeHeaderLogicColorB = s.value("nodeHeaderLogicColorB", nodeHeaderLogicColorB);
        nodeHeaderLogicColorA = s.value("nodeHeaderLogicColorA", nodeHeaderLogicColorA);

        nodeHeaderFlowColorR = s.value("nodeHeaderFlowColorR", nodeHeaderFlowColorR);
        nodeHeaderFlowColorG = s.value("nodeHeaderFlowColorG", nodeHeaderFlowColorG);
        nodeHeaderFlowColorB = s.value("nodeHeaderFlowColorB", nodeHeaderFlowColorB);
        nodeHeaderFlowColorA = s.value("nodeHeaderFlowColorA", nodeHeaderFlowColorA);

        nodeHeaderMoreColorR = s.value("nodeHeaderMoreColorR", nodeHeaderMoreColorR);
        nodeHeaderMoreColorG = s.value("nodeHeaderMoreColorG", nodeHeaderMoreColorG);
        nodeHeaderMoreColorB = s.value("nodeHeaderMoreColorB", nodeHeaderMoreColorB);
        nodeHeaderMoreColorA = s.value("nodeHeaderMoreColorA", nodeHeaderMoreColorA);

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

    s["nodeHeaderEventColorR"] = nodeHeaderEventColorR;
    s["nodeHeaderEventColorG"] = nodeHeaderEventColorG;
    s["nodeHeaderEventColorB"] = nodeHeaderEventColorB;
    s["nodeHeaderEventColorA"] = nodeHeaderEventColorA;

    s["nodeHeaderDataColorR"] = nodeHeaderDataColorR;
    s["nodeHeaderDataColorG"] = nodeHeaderDataColorG;
    s["nodeHeaderDataColorB"] = nodeHeaderDataColorB;
    s["nodeHeaderDataColorA"] = nodeHeaderDataColorA;

    s["nodeHeaderStructColorR"] = nodeHeaderStructColorR;
    s["nodeHeaderStructColorG"] = nodeHeaderStructColorG;
    s["nodeHeaderStructColorB"] = nodeHeaderStructColorB;
    s["nodeHeaderStructColorA"] = nodeHeaderStructColorA;

    s["nodeHeaderLogicColorR"] = nodeHeaderLogicColorR;
    s["nodeHeaderLogicColorG"] = nodeHeaderLogicColorG;
    s["nodeHeaderLogicColorB"] = nodeHeaderLogicColorB;
    s["nodeHeaderLogicColorA"] = nodeHeaderLogicColorA;

    s["nodeHeaderFlowColorR"] = nodeHeaderFlowColorR;
    s["nodeHeaderFlowColorG"] = nodeHeaderFlowColorG;
    s["nodeHeaderFlowColorB"] = nodeHeaderFlowColorB;
    s["nodeHeaderFlowColorA"] = nodeHeaderFlowColorA;

    s["nodeHeaderMoreColorR"] = nodeHeaderMoreColorR;
    s["nodeHeaderMoreColorG"] = nodeHeaderMoreColorG;
    s["nodeHeaderMoreColorB"] = nodeHeaderMoreColorB;
    s["nodeHeaderMoreColorA"] = nodeHeaderMoreColorA;

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