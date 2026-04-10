#include "editor/settings.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <cmath>

namespace Settings
{

static constexpr const char* k_path = "settings.json";

// Returns a deterministic default RGBA color for a category name.
// Known categories use their historical colors; others get a hash-based hue.
static std::array<float, 4> GenerateDefaultColor(const std::string& category)
{
    static const std::unordered_map<std::string, std::array<float, 4>> kDefaults = {
        { "Events",  { 0.220f, 0.340f, 0.760f, 1.000f } },
        { "Data",    { 0.180f, 0.520f, 0.420f, 1.000f } },
        { "Structs", { 0.900f, 0.520f, 0.180f, 1.000f } },
        { "Logic",   { 0.820f, 0.520f, 0.200f, 1.000f } },
        { "Flow",    { 0.770f, 0.300f, 0.320f, 1.000f } },
        { "Demo",    { 0.320f, 0.320f, 0.360f, 1.000f } },
        { "Render",  { 0.300f, 0.500f, 0.550f, 1.000f } },
        { "Math",    { 0.200f, 0.560f, 0.540f, 1.000f } },
    };

    auto it = kDefaults.find(category);
    if (it != kDefaults.end())
        return it->second;

    // Hash-based HSV color for any other category (s=0.55, v=0.50)
    const size_t h = std::hash<std::string>{}(category);
    const float hue = static_cast<float>(h % 360) / 360.0f;
    const float s = 0.55f;
    const float v = 0.50f;
    const float c = v * s;
    const float x = c * (1.0f - std::fabs(std::fmod(hue * 6.0f, 2.0f) - 1.0f));
    const float m = v - c;
    float r = 0, g = 0, b = 0;
    switch (static_cast<int>(hue * 6.0f) % 6)
    {
        case 0: r = c; g = x; b = 0; break;
        case 1: r = x; g = c; b = 0; break;
        case 2: r = 0; g = c; b = x; break;
        case 3: r = 0; g = x; b = c; break;
        case 4: r = x; g = 0; b = c; break;
        default: r = c; g = 0; b = x; break;
    }
    return { r + m, g + m, b + m, 1.0f };
}

std::array<float, 4> GetNodeHeaderColor(const std::string& category)
{
    auto it = nodeHeaderColors.find(category);
    if (it != nodeHeaderColors.end())
        return it->second;

    auto color = GenerateDefaultColor(category);
    nodeHeaderColors[category] = color;
    return color;
}

void Load()
{
    std::ifstream file(k_path);
    if (!file.is_open())
        return; // First run or no settings file: keep defaults.

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

        // Node header colors — dynamic map stored as a JSON object
        if (s.contains("nodeHeaderColors") && s["nodeHeaderColors"].is_object())
        {
            for (auto& [key, val] : s["nodeHeaderColors"].items())
            {
                if (val.is_array() && val.size() == 4)
                    nodeHeaderColors[key] = { val[0].get<float>(), val[1].get<float>(), val[2].get<float>(), val[3].get<float>() };
            }
        }
        else
        {
            // Migrate from the old per-component key format (settings.json written by an older build)
            auto readOld = [&](const char* r, const char* g, const char* b, const char* a, std::array<float, 4> def) -> std::array<float, 4>
            {
                return { s.value(r, def[0]), s.value(g, def[1]), s.value(b, def[2]), s.value(a, def[3]) };
            };
            nodeHeaderColors["Events"]  = readOld("nodeHeaderEventColorR",  "nodeHeaderEventColorG",  "nodeHeaderEventColorB",  "nodeHeaderEventColorA",  GenerateDefaultColor("Events"));
            nodeHeaderColors["Data"]    = readOld("nodeHeaderDataColorR",   "nodeHeaderDataColorG",   "nodeHeaderDataColorB",   "nodeHeaderDataColorA",   GenerateDefaultColor("Data"));
            nodeHeaderColors["Structs"] = readOld("nodeHeaderStructColorR", "nodeHeaderStructColorG", "nodeHeaderStructColorB", "nodeHeaderStructColorA", GenerateDefaultColor("Structs"));
            nodeHeaderColors["Logic"]   = readOld("nodeHeaderLogicColorR",  "nodeHeaderLogicColorG",  "nodeHeaderLogicColorB",  "nodeHeaderLogicColorA",  GenerateDefaultColor("Logic"));
            nodeHeaderColors["Flow"]    = readOld("nodeHeaderFlowColorR",   "nodeHeaderFlowColorG",   "nodeHeaderFlowColorB",   "nodeHeaderFlowColorA",   GenerateDefaultColor("Flow"));
            nodeHeaderColors["Demo"]    = readOld("nodeHeaderMoreColorR",   "nodeHeaderMoreColorG",   "nodeHeaderMoreColorB",   "nodeHeaderMoreColorA",   GenerateDefaultColor("Demo"));
        }

        // Add new settings here.
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

    // Node header colors — write as a JSON object keyed by category name
    nlohmann::json colors = nlohmann::json::object();
    for (auto& [category, rgba] : nodeHeaderColors)
        colors[category] = { rgba[0], rgba[1], rgba[2], rgba[3] };
    s["nodeHeaderColors"] = colors;

    // Add new settings here.

    std::ofstream file(k_path);
    if (!file.is_open())
    {
        std::cerr << "[Settings] Failed to write settings.json\n";
        return;
    }

    file << s.dump(4);
}

} // namespace Settings
