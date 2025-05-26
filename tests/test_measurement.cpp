#include <gtest/gtest.h>
#include <atomic>
#include <future>
#include "measurement.h"

static auto dummyCallback = [](const std::vector<double> &,
                               const std::vector<double> &,
                               const std::vector<double> &,
                               const std::vector<double> &,
                               const std::vector<double> &) {};

TEST(MeasurementTest, RunFillsDataCorrectly)
{
    std::promise<void> doneSignal;
    auto future = doneSignal.get_future();

    std::vector<double> lastFreqs;
    auto update = [&](const std::vector<double> &freqs,
                      const std::vector<double> &, const std::vector<double> &,
                      const std::vector<double> &, const std::vector<double> &)
    {
        lastFreqs = freqs;
    };

    auto done = [&](const std::vector<double> &freqs,
                    const std::vector<double> &, const std::vector<double> &,
                    const std::vector<double> &, const std::vector<double> &)
    {
        lastFreqs = freqs;
        doneSignal.set_value();
    };

    Measurement m(2.85, 2.87, 5, 0.001, 2, 1, update, done, false, VI_NULL, VI_NULL);
    m.start();
    future.wait(); // Wait for run() to complete
    m.stop();

    ASSERT_EQ(lastFreqs.size(), 5);
    EXPECT_NEAR(lastFreqs.front(), 2.85, 1e-6);
    EXPECT_NEAR(lastFreqs.back(), 2.87, 1e-6);
}

TEST(MeasurementTest, StartStopsThreadSafely)
{
    Measurement m(2.85, 2.87, 5, 0.001, 2, 1, dummyCallback, dummyCallback, false, VI_NULL, VI_NULL);
    m.start();
    m.stop();
    SUCCEED();
}

TEST(MeasurementTest, HandlesZeroStepsGracefully)
{
    std::atomic<bool> called{false};

    auto done = [&](const std::vector<double> &freqs,
                    const std::vector<double> &, const std::vector<double> &,
                    const std::vector<double> &, const std::vector<double> &)
    {
        called = true;
        EXPECT_TRUE(freqs.empty());
    };

    Measurement m(2.85, 2.87, 0, 0.001, 1, 1, dummyCallback, done, false, VI_NULL, VI_NULL);
    m.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m.stop();
    EXPECT_TRUE(called);
}

TEST(MeasurementTest, HandlesZeroAveragesGracefully)
{
    std::atomic<bool> called{false};

    auto done = [&](const std::vector<double> &, const std::vector<double> &,
                    const std::vector<double> &, const std::vector<double> &,
                    const std::vector<double> &)
    {
        called = true;
    };

    Measurement m(2.85, 2.87, 3, 0.001, 0, 1, dummyCallback, done, false, VI_NULL, VI_NULL);
    m.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m.stop();
    EXPECT_TRUE(called);
}

TEST(MeasurementTest, StandardDeviationIsNonNegative)
{
    std::promise<void> doneSignal;
    auto future = doneSignal.get_future();

    std::vector<double> stddevs;
    auto done = [&](const std::vector<double> &,
                    const std::vector<double> &,
                    const std::vector<double> &,
                    const std::vector<double> &,
                    const std::vector<double> &sd)
    {
        stddevs = sd;
        doneSignal.set_value();
    };

    Measurement m(2.85, 2.87, 5, 0.001, 2, 1, dummyCallback, done, false, VI_NULL, VI_NULL);
    m.start();
    future.wait();
    m.stop();

    for (const auto &sd : stddevs)
    {
        EXPECT_GE(sd, 0.0);
    }
}

TEST(MeasurementTest, UpdateIntervalZeroCallsUpdateOnlyAtEnd)
{
    std::promise<void> finished;
    std::atomic<int> updates{0};

    auto onUpdate = [&](const auto &...)
    { ++updates; };
    auto onDone = [&](const auto &...)
    { finished.set_value(); };

    Measurement m(2.85, 2.86,
                  4,
                  0.0005,
                  2,
                  0,
                  onUpdate, onDone,
                  false, VI_NULL, VI_NULL);

    m.start();
    finished.get_future().wait();
    m.stop();

    EXPECT_EQ(updates.load(), 1);
}

TEST(MeasurementTest, EarlyStopBreaksOutOfMainLoop)
{
    std::promise<void> done;
    std::vector<double> freqsAtDone;

    auto onDone = [&](const std::vector<double> &f,
                      const auto &...)
    {
        freqsAtDone = f;
        done.set_value();
    };

    const int bigSteps = 100;
    Measurement m(2.85, 2.95,
                  bigSteps,
                  0.001, 1, 10, [](const auto &...) {}, // dummy update
                  onDone, false, VI_NULL, VI_NULL);

    m.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    m.stop();
    done.get_future().wait();

    EXPECT_LT(freqsAtDone.size(), bigSteps); // loop exited early
}

TEST(MeasurementTest, RestartJoinsPreviousThreadSafely)
{
    std::promise<void> done1, done2;
    std::atomic<int> completes{0};

    auto makeDone = [&](std::promise<void> &p)
    {
        return [&](const auto &...)
        {
            ++completes;
            p.set_value();
        };
    };

    Measurement m(2.85, 2.87, 5, 0.0005, 1, 1, [](const auto &...) {}, makeDone(done1), false, VI_NULL, VI_NULL);

    // first run
    m.start();
    done1.get_future().wait(); // measurement finished naturally

    // second run without calling stop() first
    m.setOnDone(makeDone(done2));
    m.start(); // should .join() the old thread
    done2.get_future().wait();

    m.stop(); // safe even if already joined

    EXPECT_EQ(completes.load(), 2); // both runs completed
}
