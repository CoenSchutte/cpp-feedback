#pragma once
#include <vector>
#include <atomic>
#include <thread>
#include <functional>
#include "visa.h"

class Measurement
{
public:
    using Callback = std::function<void(const std::vector<double> &freqs,
                                        const std::vector<double> &avg,
                                        const std::vector<double> &min,
                                        const std::vector<double> &max,
                                        const std::vector<double> &stddev)>;

    Measurement(double startFreq, double endFreq, int steps, double delay,
                int averages, int updateInterval,
                Callback onUpdate, Callback onDone,
                bool useVisa,
                ViSession rigolSession,
                ViSession keithleySession);

    ~Measurement();

    void start();
    void stop();
    void setOnDone(Callback cb) { onDone = cb; }

private:
    void run();
    double simulateODMR(double freq);

    std::atomic<bool> stopFlag;
    std::thread worker;
    double startFreq, endFreq, delay;
    int steps, averages, updateInterval;
    Callback onUpdate, onDone;

    bool useVisa = false;
    ViSession keithleySession = VI_NULL;
    ViSession rigolSession = VI_NULL;
};
