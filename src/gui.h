#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <imgui.h>
#include "implot.h"
#include "implot3d.h"

#include <visa.h>
#include "measurement.h"

class Gui
{
public:
    Gui();
    void Render();

    enum class ThemeMode
    {
        Light,
        Dark
    };

    void ApplyTheme(ThemeMode mode);

private:
    void RenderControls();
    void RenderPlots();
    void RenderSettingsWindow();

    void RenderSweepSettings();
    void RenderPlotSettings();
    void RenderMeasurementStats();
    void RenderExportButton();
    void Render2DPlot();
    void Render3DPlot();
    void RenderPositioningSettings();
    void RenderPositioningPlot();

    void StartMeasurement();
    void StopMeasurement();

    void RenderVisaPanel();
    void DiscoverVisaDevices();
    void ConnectVisaDevices();

    ImVec4 GetThemeColor(const std::string &role) const;

    // Measurement Parameters
    float startFreq{};
    float endFreq{};
    float delay{};
    int steps{};
    int averages{};
    int updateInterval{};

    // Plotting & State
    float lineWidth{};
    float minSignal{std::numeric_limits<float>::max()};
    float maxSignal{std::numeric_limits<float>::lowest()};
    float lastFreq{0.0f};
    float lastSignal{0.0f};
    int totalPoints{};
    int measurementCount{};
    int plotStyle{}; // 0 = Line, 1 = Scatter
    bool isRunning{false};
    bool shouldAutoFit{false};

    // Timing
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point endTime;

    // Plot Data Buffers
    struct PlotData
    {
        std::vector<double> x;
        std::vector<double> y;
        std::vector<double> yMin;
        std::vector<double> yMax;
        std::vector<double> yStd;
        std::vector<double> z;
    };

    PlotData frontBuffer;
    PlotData backBuffer;

    std::mutex bufferMutex;
    bool hasNewData{false};

    // Measurement Results
    std::vector<double> xData;
    std::vector<double> yData;
    std::vector<double> zData;
    std::vector<double> yMinData;
    std::vector<double> yMaxData;
    std::vector<double> yStdData;

    std::vector<std::vector<double>> xLines3D;
    std::vector<std::vector<double>> yLines3D;
    std::vector<std::vector<double>> zLines3D;

    std::unique_ptr<Measurement> measurement;

    enum class UncertaintyBandMode
    {
        None = 0,
        MinMax,
        StdDev,
        Both
    };

    UncertaintyBandMode bandMode{UncertaintyBandMode::MinMax};

    bool showSettingsWindow{false};

    std::mutex dataMutex;

    struct VisaDevice
    {
        std::string address;
    };

    std::vector<VisaDevice> visaDevices;

    int selectedRigolIndex{-1};
    int selectedKeithleyIndex{-1};

    bool rigolConnected{false};
    bool keithleyConnected{false};

    ViSession rigolSession{VI_NULL};
    ViSession keithleySession{VI_NULL};

    bool useVisaMode{false};

    ThemeMode currentTheme{ThemeMode::Dark};

    enum class PlotTab
    {
        None,
        Plot2D,
        Plot3D,
        Positioning2D
    };

    PlotTab currentPlotTab{PlotTab::None};
};
