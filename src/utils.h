#pragma once
#include <vector>
#include <thread>
#include <chrono>

namespace odmr_utils
{
    void accurate_sleep(double duration_seconds);
    std::vector<int> FindProminentDips(const std::vector<double> &y, int window, double prominence);

}
