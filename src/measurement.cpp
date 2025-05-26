#include "measurement.h"
#include <cmath>
#include <random>
#include <chrono>
#include <thread>
#include "utils.h"
#include <iostream>

Measurement::Measurement(double sf, double ef, int s, double d, int a, int u,
                         Callback update, Callback done,
                         bool visaEnabled,
                         ViSession rigol,
                         ViSession keithley)
    : stopFlag(false),
      startFreq(sf), endFreq(ef), delay(d),
      steps(s), averages(a), updateInterval(u),
      onUpdate(update), onDone(done),
      useVisa(visaEnabled),
      keithleySession(keithley),
      rigolSession(rigol)
{
}

Measurement::~Measurement()
{
    stop(); // Ensures the thread is joined
}

void Measurement::start()
{

    if (worker.joinable())
    {
        worker.join();
    }

    stopFlag = false;
    worker = std::thread(&Measurement::run, this);
}

void Measurement::stop()
{
    stopFlag = true;
    if (worker.joinable())
        worker.join();
}

double Measurement::simulateODMR(double freq)
{
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_real_distribution<> dis(-0.01, 0.01);

    const double center1 = 2.87;
    const double center2 = 2.90;
    const double width = 0.005;

    double dip1 = 0.2 * (1.0 / (1.0 + std::pow((freq - center1) / width, 2)));
    double dip2 = 0.2 * (1.0 / (1.0 + std::pow((freq - center2) / width, 2)));

    return 1.0 - dip1 - dip2 + dis(gen);
}

void Measurement::run()
{
    std::vector<double> freqs, avgSignals, minSignals, maxSignals, stdDevs;

    for (int i = 0; i < steps; ++i)
    {
        if (stopFlag)
            break;

        double freqGHz = startFreq + i * (endFreq - startFreq) / (steps - 1);
        double freqHz = freqGHz * 1e9;

        // Set frequency on Rigol
        if (useVisa && rigolSession != VI_NULL)
        {
            char cmd[64];
            snprintf(cmd, sizeof(cmd), ":FREQ %.0f\n", freqHz); // in Hz
            ViStatus s = viWrite(rigolSession, (ViBuf)cmd, (ViUInt32)strlen(cmd), VI_NULL);
            if (s < VI_SUCCESS)
                std::cerr << "Failed to set frequency on Rigol\n";
        }

        // Average readings from Keithley
        double sum = 0.0, sumSq = 0.0;
        double minVal = std::numeric_limits<double>::max();
        double maxVal = std::numeric_limits<double>::lowest();

        for (int j = 0; j < averages; ++j)
        {
            double voltage = 0.0;

            if (useVisa && keithleySession != VI_NULL)
            {
                viSetAttribute(keithleySession, VI_ATTR_TMO_VALUE, 1000); // 1 second timeout TODO
                ViStatus status = viQueryf(keithleySession, (ViString) "MEAS:VOLT:DC?\n", (ViString) "%lf", &voltage);
                if (status < VI_SUCCESS)
                {
                    voltage = 0.0;
                    std::cerr << "Keithley read failed at step " << i << "\n";
                }
            }
            else
            {
                voltage = simulateODMR(freqGHz); // fallback
            }

            sum += voltage;
            sumSq += voltage * voltage;
            minVal = std::min(minVal, voltage);
            maxVal = std::max(maxVal, voltage);

            odmr_utils::accurate_sleep(delay / averages); // spread delay
        }

        double avg = sum / averages;
        double variance = (sumSq / averages) - (avg * avg);
        double stddev = std::sqrt(std::max(variance, 0.0));

        freqs.push_back(freqGHz);
        avgSignals.push_back(avg);
        minSignals.push_back(minVal);
        maxSignals.push_back(maxVal);
        stdDevs.push_back(stddev);

        if (updateInterval > 0 && i % updateInterval == 0)
        {
            onUpdate(freqs, avgSignals, minSignals, maxSignals, stdDevs);
        }
    }

    onUpdate(freqs, avgSignals, minSignals, maxSignals, stdDevs);
    if (onDone)
        onDone(freqs, avgSignals, minSignals, maxSignals, stdDevs);
}
