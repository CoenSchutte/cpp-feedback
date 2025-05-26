#pragma once
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

struct UserSettings
{
    float startFreq = 2.85f;
    float endFreq = 2.92f;
    float delay = 0.001f;
    int steps = 1000;
    int averages = 10;
    int updateInterval = 1;
    float lineWidth = 1.5f;
    int plotStyle = 0;
    int bandMode = 1;
    int themeMode = 1; // 0 = Light, 1 = Dark

    nlohmann::json to_json() const;
    void from_json(const nlohmann::json &j);
};

class SettingsManager
{
public:
    static SettingsManager &Instance();

    void Load();
    void Save();
    const UserSettings &Get() const;
    UserSettings &Edit(); // editable version
    std::filesystem::path GetPath() const;

    void SetCustomPath(const std::filesystem::path &path);

private:
    SettingsManager() = default;
    UserSettings settings;

    std::optional<std::filesystem::path> customPath;
};
