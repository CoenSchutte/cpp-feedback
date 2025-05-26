#include "gui.h"
#include <implot.h>
#include <implot3d.h>
#include <imgui.h>
#include <iomanip>
#include <sstream>
#include <fstream>
#include "utils.h"
#include "settings.h"
#include <string>

Gui::Gui()
{
    SettingsManager::Instance().Load();
    const auto &s = SettingsManager::Instance().Get();

    startFreq = s.startFreq;
    endFreq = s.endFreq;
    delay = s.delay;
    steps = s.steps;
    averages = s.averages;
    updateInterval = s.updateInterval;
    lineWidth = s.lineWidth;
    plotStyle = s.plotStyle;
    bandMode = static_cast<UncertaintyBandMode>(s.bandMode);

    ApplyTheme(static_cast<ThemeMode>(s.themeMode));
}

void Gui::Render()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Settings"))
            {
                showSettingsWindow = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (showSettingsWindow)
    {
        RenderSettingsWindow();
    }

    RenderControls();
    RenderPlots();
}

void Gui::RenderControls()
{
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
    ImGui::Begin("ODMR Controls", nullptr, windowFlags);

    if (isRunning)
        ImGui::BeginDisabled();

    if (currentPlotTab == PlotTab::Plot2D || currentPlotTab == PlotTab::Plot3D)
    {
        RenderSweepSettings();
        RenderPlotSettings();
        RenderVisaPanel();
    }

    if (isRunning)
        ImGui::EndDisabled();

    if (!isRunning && ImGui::Button("Start", ImVec2(120, 40)))
        StartMeasurement();
    if (isRunning && ImGui::Button("Stop", ImVec2(120, 40)))
        StopMeasurement();

    ImGui::Spacing();
    float progress = (totalPoints > 0) ? (float)measurementCount / (float)totalPoints : 0.0f;
    char progressLabel[64];
    snprintf(progressLabel, sizeof(progressLabel), "Sweep Progress: %d / %d", measurementCount, totalPoints);
    ImGui::ProgressBar(progress, ImVec2(-1, 0), progressLabel);

    ImGui::Spacing();
    ImGui::Separator();

    RenderMeasurementStats();
    RenderExportButton();

    ImGui::End();
}

void Gui::RenderPlots()
{
    // Handle new measurement data with double-buffer swap
    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        if (hasNewData)
        {
            std::swap(frontBuffer, backBuffer);
            hasNewData = false;

            std::lock_guard<std::mutex> dataLock(dataMutex);
            xData = frontBuffer.x;
            yData = frontBuffer.y;
            yMinData = frontBuffer.yMin;
            yMaxData = frontBuffer.yMax;
            yStdData = frontBuffer.yStd;
            zData = frontBuffer.z;

            measurementCount = static_cast<int>(xData.size());
            if (!xData.empty())
            {
                lastFreq = static_cast<float>(xData.back());
                lastSignal = static_cast<float>(yData.back());
            }

            for (double s : yData)
            {
                minSignal = std::min(minSignal, static_cast<float>(s));
                maxSignal = std::max(maxSignal, static_cast<float>(s));
            }

            shouldAutoFit = true;
        }
    }

    // Continue rendering plots as normal
    ImGui::Begin("ODMR Plot");

    if (ImGui::BeginTabBar("PlotTabs"))
    {
        if (ImGui::BeginTabItem("2D Plot"))
        {
            currentPlotTab = PlotTab::Plot2D;
            Render2DPlot();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("3D Plot"))
        {
            currentPlotTab = PlotTab::Plot3D;
            Render3DPlot();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    else
    {
        currentPlotTab = PlotTab::None;
    }

    ImGui::End();
}

void Gui::Render2DPlot()
{
    if (shouldAutoFit)
    {
        ImPlot::SetNextAxesToFit();
        shouldAutoFit = false;
    }

    if (ImPlot::BeginPlot("ODMR Spectrum", ImVec2(-1, -1)))
    {
        ImPlot::SetupAxes("Frequency (GHz)", "Signal");
        ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, lineWidth);

        std::lock_guard<std::mutex> lock(dataMutex);
        if (!xData.empty())
        {
            ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.2f);

            if ((bandMode == UncertaintyBandMode::MinMax || bandMode == UncertaintyBandMode::Both) &&
                !yMinData.empty() && !yMaxData.empty())
            {
                ImPlot::PlotShaded("Min/Max Band", xData.data(), yMinData.data(), yMaxData.data(), static_cast<int>(xData.size()));
            }

            if ((bandMode == UncertaintyBandMode::StdDev || bandMode == UncertaintyBandMode::Both) &&
                !yStdData.empty())
            {
                std::vector<double> yLower, yUpper;
                yLower.reserve(yData.size());
                yUpper.reserve(yData.size());
                for (size_t i = 0; i < yData.size(); ++i)
                {
                    yLower.push_back(yData[i] - yStdData[i]);
                    yUpper.push_back(yData[i] + yStdData[i]);
                }
                ImPlot::PlotShaded("Std Dev Band", xData.data(), yLower.data(), yUpper.data(), static_cast<int>(xData.size()));
            }

            ImPlot::PopStyleVar();

            if (plotStyle == 0)
                ImPlot::PlotLine("Signal", xData.data(), yData.data(), static_cast<int>(xData.size()));
            else
                ImPlot::PlotScatter("Signal", xData.data(), yData.data(), static_cast<int>(xData.size()));

            static float dipProminence = 0.01f;
            static int dipWindow = 40;
            std::vector<int> dips = odmr_utils::FindProminentDips(yData, dipWindow, dipProminence);

            ImPlot::PushStyleColor(ImPlotCol_MarkerFill, ImVec4(1, 0, 0, 1));
            ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_Circle);
            double dummyX = 0.0, dummyY = 0.0;
            ImPlot::PlotScatter("Dips", &dummyX, &dummyY, 0);
            ImPlot::PopStyleVar();
            ImPlot::PopStyleColor();

            if (!dips.empty())
            {
                std::vector<double> dipXs, dipYs;
                dipXs.reserve(dips.size());
                dipYs.reserve(dips.size());
                for (int idx : dips)
                {
                    dipXs.push_back(xData[idx]);
                    dipYs.push_back(yData[idx]);
                }
                ImPlot::PlotScatter("Dips", dipXs.data(), dipYs.data(), static_cast<int>(dipXs.size()));
            }
        }

        ImPlot::PopStyleVar();
        ImPlot::EndPlot();
    }
}

void Gui::Render3DPlot()
{
    std::lock_guard<std::mutex> lock(dataMutex);

    if (!xLines3D.empty() && ImPlot3D::BeginPlot("ODMR 3D View", ImVec2(-1, -1)))
    {
        ImPlot3D::SetupAxes("Time (s)", "Frequency (GHz)", "Signal");
        ImPlot3D::PushStyleVar(ImPlot3DStyleVar_MarkerSize, 4.0f);

        std::vector<double> dipXs, dipYs, dipZs;
        static float dipProminence = 0.01f;
        static int dipWindow = 40;

        for (size_t i = 0; i < xLines3D.size(); ++i)
        {
            const auto &x = xLines3D[i];
            const auto &y = yLines3D[i];
            const auto &z = zLines3D[i];

            if (x.size() == y.size() && y.size() == z.size())
            {
                std::string label = "Sweep " + std::to_string(i + 1);
                ImPlot3D::PlotLine(label.c_str(), z.data(), x.data(), y.data(), static_cast<int>(x.size()));

                std::vector<int> dips = odmr_utils::FindProminentDips(y, dipWindow, dipProminence);
                for (int idx : dips)
                {
                    dipXs.push_back(x[idx]);
                    dipYs.push_back(y[idx]);
                    dipZs.push_back(z[idx]);
                }
            }
        }

        if (!dipXs.empty())
        {
            ImPlot3D::PushStyleColor(ImPlotCol_MarkerFill, IM_COL32(255, 0, 0, 255));
            ImPlot3D::PlotScatter("Dip", dipZs.data(), dipXs.data(), dipYs.data(), static_cast<int>(dipXs.size()));
            ImPlot3D::PopStyleColor();
        }

        ImPlot3D::PopStyleVar();
        ImPlot3D::EndPlot();
    }
}

void Gui::RenderSweepSettings()
{
    if (ImGui::CollapsingHeader("Sweep Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat("Start Freq (GHz)", &startFreq, 2.85e9, 2.90e9);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Frequency sweep start");

        ImGui::SliderFloat("End Freq (GHz)", &endFreq, 2.90e9, 3.00e9);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Frequency sweep end");

        ImGui::InputInt("Steps", &steps);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Number of points in the sweep");

        ImGui::InputInt("Averages", &averages);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Number of averages per point");

        ImGui::SliderFloat("Delay (s)", &delay, 0.0001f, 0.01f, "%.4f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Delay between points");

        ImGui::InputInt("Update Interval", &updateInterval);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Interval of points for updating the 2D plot");
    }
}

void Gui::RenderPlotSettings()
{
    static float dipProminence = 0.01f;
    static int dipWindow = 40;

    if (ImGui::CollapsingHeader("Plot Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat("Line Width", &lineWidth, 0.1f, 3.0f);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Width of the plot lines");

        ImGui::Combo("Plot Style", &plotStyle, "Line\0Scatter\0\0");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select the plot style");

        ImGui::SliderInt("Dip Window", &dipWindow, 1, 50);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Window size for dip detection");

        ImGui::SliderFloat("Dip Prominence", &dipProminence, 0.0001f, 0.02f);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Minimum prominence for dip detection");

        static const char *bandOptions[] = {"None", "Min/Max", "Std Dev", "Both"};
        static int bandModeIdx = static_cast<int>(bandMode);
        if (ImGui::Combo("Uncertainty Band", &bandModeIdx, bandOptions, IM_ARRAYSIZE(bandOptions)))
        {
            bandMode = static_cast<UncertaintyBandMode>(bandModeIdx);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select the type of uncertainty band to display");

        ImGui::Separator();
    }
}

void Gui::RenderMeasurementStats()
{
    ImGui::BeginChild("MeasurementStats", ImVec2(0, 130), true);
    ImGui::Text("Status: ");
    ImGui::SameLine();
    ImGui::TextColored(isRunning ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                       isRunning ? "Running" : "Stopped");
    ImGui::Text("Last Frequency: %.5f GHz", lastFreq);
    ImGui::Text("Signal: %.4f", lastSignal);
    ImGui::Text("Min / Max: %.3f / %.3f", minSignal, maxSignal);

    float elapsed = 0.0f;
    if (startTime.time_since_epoch().count() > 0)
    {
        auto end = (isRunning || endTime.time_since_epoch().count() == 0)
                       ? std::chrono::high_resolution_clock::now()
                       : endTime;
        elapsed = std::chrono::duration<float>(end - startTime).count();
    }
    float remaining = (totalPoints - measurementCount) * delay;
    ImGui::Text("Elapsed: %.3f s", elapsed);
    ImGui::Text("Estimated Remaining: %.1f s", isRunning ? remaining : 0.0f);
    ImGui::EndChild();
}

void Gui::RenderExportButton()
{
    static bool exportSuccess = false;
    static std::string lastExportFilename;
    static std::string exportErrorMessage;

    if (!xData.empty() && ImGui::Button("Export CSV"))
    {
        exportSuccess = false;
        exportErrorMessage.clear();
        lastExportFilename.clear();

        int fileIndex = 1;
        std::string baseName = "odmr_export_";
        std::string filename;
        do
        {
            filename = baseName + std::to_string(fileIndex++) + ".csv";
            std::ifstream checkFile(filename);
            if (!checkFile.good())
                break;
        } while (true);

        try
        {
            std::ofstream outFile(filename);
            outFile << "Frequency (GHz),Signal,Time (s)\n";
            for (size_t i = 0; i < xData.size(); ++i)
                outFile << xData[i] << "," << yData[i] << "," << zData[i] << "\n";
            outFile.close();
            exportSuccess = true;
            lastExportFilename = filename;
        }
        catch (const std::exception &e)
        {
            exportErrorMessage = std::string("Error writing CSV: ") + e.what();
        }
    }

    if (exportSuccess)
        ImGui::TextColored(GetThemeColor("success"), "Exported: %s", lastExportFilename.c_str());
    else if (!exportErrorMessage.empty())
        ImGui::TextColored(GetThemeColor("error"), "%s", exportErrorMessage.c_str());
}

void Gui::StartMeasurement()
{
    if (isRunning && measurement)
    {
        measurement->stop();
        measurement.reset();
        isRunning = false;
    }

    isRunning = true;
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        xData.clear();
        yData.clear();
        zData.clear();
        yMinData.clear();
        yMaxData.clear();
        yStdData.clear();
    }

    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        frontBuffer = {};
        backBuffer = {};
        hasNewData = false;
    }

    minSignal = std::numeric_limits<float>::max();
    maxSignal = std::numeric_limits<float>::lowest();
    lastFreq = 0.0f;
    lastSignal = 0.0f;
    measurementCount = 0;
    totalPoints = steps;
    startTime = std::chrono::high_resolution_clock::now();
    endTime = {};

    measurement = std::make_unique<Measurement>(
        startFreq, endFreq, steps, delay, averages, updateInterval,
        [this](const std::vector<double> &x,
               const std::vector<double> &y,
               const std::vector<double> &yMin,
               const std::vector<double> &yMax,
               const std::vector<double> &stddev)
        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            backBuffer.x = x;
            backBuffer.y = y;
            backBuffer.yMin = yMin;
            backBuffer.yMax = yMax;
            backBuffer.yStd = stddev;
            double t = std::chrono::duration<double>(
                           std::chrono::high_resolution_clock::now() - startTime)
                           .count();
            backBuffer.z.resize(y.size(), t);
            hasNewData = true;
        },
        [this](const std::vector<double> &x,
               const std::vector<double> &y,
               const std::vector<double> &yMin [[maybe_unused]],
               const std::vector<double> &yMax [[maybe_unused]],
               const std::vector<double> &stddev)
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            double t = std::chrono::duration<double>(
                           std::chrono::high_resolution_clock::now() - startTime)
                           .count();
            std::vector<double> z(y.size(), t);
            xLines3D.push_back(x);
            yLines3D.push_back(y);
            zLines3D.push_back(std::move(z));
            zData = z;
            yStdData = stddev;
            isRunning = false;
            endTime = std::chrono::high_resolution_clock::now();
        },
        useVisaMode,
        rigolSession,
        keithleySession);

    measurement->start();
}

void Gui::StopMeasurement()
{
    if (measurement)
    {
        measurement->stop();
        measurement.reset();
    }

    isRunning = false;

    // Reset double-buffered data
    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        hasNewData = false;
        frontBuffer = {};
        backBuffer = {};
    }

    // Clear shared plot data
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        xData.clear();
        yData.clear();
        zData.clear();
        yMinData.clear();
        yMaxData.clear();
        yStdData.clear();
    }

    //  Reset display state
    minSignal = std::numeric_limits<float>::max();
    maxSignal = std::numeric_limits<float>::lowest();
    lastFreq = 0.0f;
    lastSignal = 0.0f;
    measurementCount = 0;
    totalPoints = 0;
    startTime = {};
    endTime = {};
}

void Gui::RenderSettingsWindow()
{
    auto &settings = SettingsManager::Instance().Edit();
    ImGui::Begin("Settings", &showSettingsWindow);

    ImGui::SliderFloat("Start Freq", &settings.startFreq, 1e6, 10e6);
    ImGui::SliderFloat("End Freq", &settings.endFreq, 1e6, 10e6);
    ImGui::SliderFloat("Delay", &settings.delay, 0.0001f, 0.01f, "%.4f");
    ImGui::InputInt("Steps", &settings.steps);
    ImGui::InputInt("Averages", &settings.averages);
    ImGui::InputInt("Update Interval", &settings.updateInterval);
    ImGui::SliderFloat("Line Width", &settings.lineWidth, 0.1f, 3.0f);
    ImGui::Combo("Plot Style", &settings.plotStyle, "Line\0Scatter\0");
    ImGui::Combo("Band Mode", &settings.bandMode, "None\0Min/Max\0Std Dev\0Both\0");

    const char *themes[] = {"Light", "Dark"};
    int &selected = settings.themeMode;
    if (ImGui::Combo("Theme", &selected, themes, IM_ARRAYSIZE(themes)))
    {
        ApplyTheme(static_cast<ThemeMode>(selected));
        SettingsManager::Instance().Save();
    }

    if (ImGui::Button("Save Settings"))
    {
        SettingsManager::Instance().Save();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    ImGui::End();
}

void Gui::ApplyTheme(ThemeMode mode)
{
    currentTheme = mode;

    ImGuiStyle &style = ImGui::GetStyle();
    ImVec4 *colors = style.Colors;

    if (mode == ThemeMode::Dark)
    {
        ImGui::StyleColorsDark();
        colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.13f, 0.14f, 0.18f, 1.00f);
    }
    else
    {
        ImGui::StyleColorsLight();
        colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.95f, 0.98f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.75f, 0.75f, 0.85f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.70f, 0.75f, 0.80f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.90f, 0.90f, 0.95f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.80f, 0.82f, 0.90f, 1.00f);
    }

    style.FrameRounding = 4.0f;
    style.WindowRounding = 6.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
}

void Gui::RenderVisaPanel()
{
    if (ImGui::CollapsingHeader("VISA", ImGuiTreeNodeFlags_DefaultOpen))
    {

        ImGui::Checkbox("Use VISA for Measurement", &useVisaMode);

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Enable to read actual voltage from connected VISA device");

        if (!useVisaMode)
            ImGui::BeginDisabled();

        if (ImGui::Button("Scan for Devices"))
        {
            DiscoverVisaDevices();
        }

        if (!visaDevices.empty())
        {
            std::vector<const char *> items;
            for (const auto &dev : visaDevices)
                items.push_back(dev.address.c_str());

            ImGui::Text("Select Rigol (Signal Generator):");
            ImGui::Combo("Rigol Device", &selectedRigolIndex, items.data(), static_cast<int>(items.size()));

            ImGui::Text("Select Keithley (Multimeter):");
            ImGui::Combo("Keithley Device", &selectedKeithleyIndex, items.data(), static_cast<int>(items.size()));

            if (ImGui::Button("Connect Devices"))
            {
                ConnectVisaDevices();
            }

            if (rigolConnected)
                ImGui::TextColored(GetThemeColor("success"), "Rigol Connected: %s", visaDevices[selectedRigolIndex].address.c_str());
            else
                ImGui::TextColored(GetThemeColor("error"), "Rigol Not Connected");

            if (keithleyConnected)
                ImGui::TextColored(GetThemeColor("success"), "Keithley Connected: %s", visaDevices[selectedKeithleyIndex].address.c_str());
            else
                ImGui::TextColored(GetThemeColor("error"), "Keithley Not Connected");
        }

        if (!useVisaMode)
            ImGui::EndDisabled();
    }
    ImGui::Spacing();
    ImGui::Separator();
}

void Gui::ConnectVisaDevices()
{
    ViSession rm;
    ViStatus status = viOpenDefaultRM(&rm);
    if (status < VI_SUCCESS)
        return;

    // Close previous sessions if needed
    if (rigolConnected && rigolSession != VI_NULL)
        viClose(rigolSession);
    if (keithleyConnected && keithleySession != VI_NULL)
        viClose(keithleySession);

    rigolConnected = false;
    keithleyConnected = false;

    if (selectedRigolIndex >= 0 && selectedRigolIndex < static_cast<int>(visaDevices.size()))
    {
        status = viOpen(rm, (ViRsrc)visaDevices[selectedRigolIndex].address.c_str(), VI_NULL, VI_NULL, &rigolSession);
        if (status >= VI_SUCCESS)
            rigolConnected = true;

        status = viOpen(rm, (ViRsrc)visaDevices[selectedKeithleyIndex].address.c_str(), VI_NULL, VI_NULL, &keithleySession);
        if (status >= VI_SUCCESS)
            keithleyConnected = true;

        // TODO MAYBE REENABLE??
        // viClose(rm);
    }
}

void Gui::DiscoverVisaDevices()
{
    ViSession rm;
    ViFindList findList;
    ViUInt32 numInstrs;
    ViChar instrDescriptor[VI_FIND_BUFLEN];
    ViStatus status;

    visaDevices.clear();
    selectedRigolIndex = -1;
    selectedKeithleyIndex = -1;

    status = viOpenDefaultRM(&rm);
    if (status < VI_SUCCESS)
        return;

    status = viFindRsrc(rm, (ViString) "?*INSTR", &findList, &numInstrs, instrDescriptor);
    if (status >= VI_SUCCESS)
    {
        visaDevices.push_back({instrDescriptor});
        for (ViUInt32 i = 1; i < numInstrs; ++i)
        {
            if (viFindNext(findList, instrDescriptor) >= VI_SUCCESS)
                visaDevices.push_back({instrDescriptor});
        }
        selectedRigolIndex = 0;
        selectedKeithleyIndex = 0;
        viClose(findList);
    }

    viClose(rm);

    viClose(rm);
}

ImVec4 Gui::GetThemeColor(const std::string &role) const
{
    if (role == "success")
        return currentTheme == ThemeMode::Light ? ImVec4(0.0f, 0.4f, 0.0f, 1.0f)
                                                : ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
    if (role == "error")
        return currentTheme == ThemeMode::Light ? ImVec4(0.6f, 0.1f, 0.1f, 1.0f)
                                                : ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
    if (role == "warn")
        return currentTheme == ThemeMode::Light ? ImVec4(1.0f, 0.55f, 0.0f, 1.0f)
                                                : ImVec4(1.0f, 0.4f, 0.0f, 1.0f);

    // Default fallback
    return ImVec4(1, 1, 1, 1);
}
