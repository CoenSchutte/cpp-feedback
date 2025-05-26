#include "settings.h"
#include <fstream>
#include <cstdlib>

using json = nlohmann::json;

json UserSettings::to_json() const
{
    return {
        {"startFreq", startFreq},
        {"endFreq", endFreq},
        {"delay", delay},
        {"steps", steps},
        {"averages", averages},
        {"updateInterval", updateInterval},
        {"lineWidth", lineWidth},
        {"plotStyle", plotStyle},
        {"bandMode", bandMode},
        {"themeMode", themeMode}};
}

void UserSettings::from_json(const json &j)
{
    if (j.contains("startFreq"))
        j.at("startFreq").get_to(startFreq);
    if (j.contains("endFreq"))
        j.at("endFreq").get_to(endFreq);
    if (j.contains("delay"))
        j.at("delay").get_to(delay);
    if (j.contains("steps"))
        j.at("steps").get_to(steps);
    if (j.contains("averages"))
        j.at("averages").get_to(averages);
    if (j.contains("updateInterval"))
        j.at("updateInterval").get_to(updateInterval);
    if (j.contains("lineWidth"))
        j.at("lineWidth").get_to(lineWidth);
    if (j.contains("plotStyle"))
        j.at("plotStyle").get_to(plotStyle);
    if (j.contains("bandMode"))
        j.at("bandMode").get_to(bandMode);
    if (j.contains("themeMode"))
        j.at("themeMode").get_to(themeMode);
}

SettingsManager &SettingsManager::Instance()
{
    static SettingsManager instance;
    return instance;
}

std::filesystem::path SettingsManager::GetPath() const
{
    if (customPath.has_value())
    {
        return *customPath;
    }

    #ifdef _WIN32
        const char *appdata = std::getenv("APPDATA");
        return std::filesystem::path(appdata ? appdata : ".") / "odmr_gui" / "settings.json";
    #elif __APPLE__
        return std::filesystem::path(std::getenv("HOME")) / "Library/Application Support/odmr_gui/settings.json";
    #else
        return std::filesystem::path(std::getenv("HOME")) / ".config/odmr_gui/settings.json";
    #endif
}

void SettingsManager::SetCustomPath(const std::filesystem::path &path)
{
    customPath = path;
}

void SettingsManager::Load()
{
    std::ifstream in(GetPath());
    if (in)
    {
        json j;
        in >> j;
        settings.from_json(j);
    }
}

void SettingsManager::Save()
{
    std::filesystem::create_directories(GetPath().parent_path());
    std::ofstream out(GetPath());
    out << settings.to_json().dump(4);
}

const UserSettings &SettingsManager::Get() const
{
    return settings;
}

UserSettings &SettingsManager::Edit()
{
    return settings;
}
