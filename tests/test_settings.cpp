#include <gtest/gtest.h>
#include "settings.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;
using namespace std::string_literals;

TEST(UserSettingsTest, SerializesToJsonCorrectly)
{
    UserSettings s;
    s.startFreq = 2.8f;
    s.endFreq = 2.9f;
    s.delay = 0.01f;
    s.steps = 200;
    s.averages = 5;
    s.updateInterval = 2;
    s.lineWidth = 2.0f;
    s.plotStyle = 1;
    s.bandMode = 0;
    s.themeMode = 0;

    json j = s.to_json();

    EXPECT_FLOAT_EQ(j["startFreq"], 2.8f);
    EXPECT_FLOAT_EQ(j["endFreq"], 2.9f);
    EXPECT_EQ(j["steps"], 200);
    EXPECT_EQ(j["themeMode"], 0);
}

TEST(UserSettingsTest, DeserializesFromJsonCorrectly)
{
    json j = {
        {"startFreq", 2.81f},
        {"endFreq", 2.91f},
        {"delay", 0.005f},
        {"steps", 150},
        {"averages", 7},
        {"updateInterval", 3},
        {"lineWidth", 1.2f},
        {"plotStyle", 2},
        {"bandMode", 1},
        {"themeMode", 1}};

    UserSettings s;
    s.from_json(j);

    EXPECT_FLOAT_EQ(s.startFreq, 2.81f);
    EXPECT_FLOAT_EQ(s.endFreq, 2.91f);
    EXPECT_EQ(s.steps, 150);
    EXPECT_EQ(s.themeMode, 1);
}

TEST(SettingsManagerTest, SingletonAccessReturnsSameInstance)
{
    SettingsManager &a = SettingsManager::Instance();
    SettingsManager &b = SettingsManager::Instance();

    EXPECT_EQ(&a, &b);
}

TEST(SettingsManagerTest, EditUpdatesAreReflectedInGet)
{
    SettingsManager &sm = SettingsManager::Instance();
    sm.Edit().startFreq = 3.00f;

    EXPECT_FLOAT_EQ(sm.Get().startFreq, 3.00f);
}

TEST(SettingsManagerTest, SaveAndLoadPersistSettings)
{
    SettingsManager &sm = SettingsManager::Instance();
    auto tempPath = std::filesystem::temp_directory_path() / "test_settings.json";
    sm.SetCustomPath(tempPath);

    sm.Edit().startFreq = 2.75f;
    sm.Edit().endFreq = 2.85f;
    sm.Save();

    // Clear and reload
    sm.Edit().startFreq = 0.0f;
    sm.Edit().endFreq = 0.0f;
    sm.Load();

    EXPECT_FLOAT_EQ(sm.Get().startFreq, 2.75f);
    EXPECT_FLOAT_EQ(sm.Get().endFreq, 2.85f);

    std::filesystem::remove(tempPath);
}


TEST(SettingsManagerTest, SavesToDefaultLocationWhenNoCustomPath)
{
    SettingsManager &sm = SettingsManager::Instance();

    // Give the settings a distinctive value so we can easily delete the file
    // afterwards if we want to.
    sm.Edit().startFreq = 42.42f;

    // No custom path has been set yet, default path is used.
    sm.Save();

    // Nothing to ASSERT here: simply reaching this point means the branch
    // executed without throwing an exception.
}


TEST(UserSettingsTest, FromJsonKeepsExistingValuesWhenKeysAreMissing)
{
    UserSettings s;
    //  Values that should survive the call:
    s.startFreq = 1.11f;
    s.endFreq = 9.99f;

    nlohmann::json partial = {// only one of keys present
                              {"startFreq", 2.22f}};

    s.from_json(partial);

    EXPECT_FLOAT_EQ(s.startFreq, 2.22f); // updated
    EXPECT_FLOAT_EQ(s.endFreq, 9.99f);   // untouched
}

TEST(SettingsManagerTest, LoadReadsBackWhatSaveWrote)
{
    SettingsManager &sm = SettingsManager::Instance();

    auto path = std::filesystem::temp_directory_path() / "settings_covered.json";
    sm.SetCustomPath(path);

    // Write something distinctive.
    sm.Edit().startFreq = 3.33f;
    sm.Edit().endFreq = 4.44f;
    sm.Save();

    // Blank out the struct, then load:
    sm.Edit().startFreq = 0.f;
    sm.Edit().endFreq = 0.f;
    sm.Load();

    EXPECT_FLOAT_EQ(sm.Get().startFreq, 3.33f);
    EXPECT_FLOAT_EQ(sm.Get().endFreq, 4.44f);

    std::filesystem::remove(path);
}

TEST(SettingsManagerTest, LoadNoFileKeepsCurrentValues)
{
    SettingsManager &sm = SettingsManager::Instance();

    auto bogus = std::filesystem::temp_directory_path() / "no_such_settings.json";
    std::filesystem::remove(bogus); // be sure it does not exist
    sm.SetCustomPath(bogus);

    sm.Edit().startFreq = 7.77f;
    sm.Edit().endFreq = 8.88f;

    sm.Load(); // should do nothing

    EXPECT_FLOAT_EQ(sm.Get().startFreq, 7.77f);
    EXPECT_FLOAT_EQ(sm.Get().endFreq, 8.88f);
}