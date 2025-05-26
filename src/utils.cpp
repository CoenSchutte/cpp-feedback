#include "utils.h"
#include <chrono>
#include <thread>

namespace odmr_utils
{
    std::vector<int> FindProminentDips(const std::vector<double> &y, int window, double prominence)
    {
        std::vector<int> rawDips;
        int n = static_cast<int>(y.size());

        for (int i = window; i < n - window; ++i)
        {
            double center = y[i];
            double localAvg = 0.0;
            for (int j = i - window; j <= i + window; ++j)
            {
                if (j == i)
                    continue;
                localAvg += y[j];
            }
            localAvg /= (2 * window);

            if (center + prominence < localAvg)
            {
                bool isMin = true;
                for (int j = i - 1; j <= i + 1; ++j)
                {
                    if (j < 0 || j >= n || j == i)
                        continue;
                    if (center > y[j])
                    {
                        isMin = false;
                        break;
                    }
                }

                if (isMin)
                    rawDips.push_back(i);
            }
        }

        std::vector<int> filteredDips;
        const int minGap = window / 2;

        for (size_t i = 0; i < rawDips.size();)
        {
            int start = rawDips[i];
            double minVal = y[start];
            int minIdx = start;

            while (i + 1 < rawDips.size() && rawDips[i + 1] - rawDips[i] <= minGap)
            {
                ++i;
                if (y[rawDips[i]] < minVal)
                {
                    minVal = y[rawDips[i]];
                    minIdx = rawDips[i];
                }
            }

            filteredDips.push_back(minIdx);
            ++i;
        }

        return filteredDips;
    }

    void accurate_sleep(double duration_seconds)
    {
        using namespace std::chrono;

        auto start = high_resolution_clock::now();
        auto end = start + duration<double>(duration_seconds);

        while (high_resolution_clock::now() < end)
        {
            auto remaining = duration_cast<microseconds>(end - high_resolution_clock::now());
            if (remaining.count() > 2000)
            {
                // Sleep only when > 2ms remaining. Prevent overshooting by / 2
                std::this_thread::sleep_for(remaining / 2);
            }
            // Otherwise spin
        }
    }
}