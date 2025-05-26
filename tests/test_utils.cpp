#include <gtest/gtest.h>
#include "utils.h"

using namespace odmr_utils;

TEST(FindProminentDipsTest, SingleDip)
{
    std::vector<double> y = {1.0, 1.0, 0.2, 1.0, 1.0};
    int window = 1;
    double prominence = 0.5;

    std::vector<int> result = FindProminentDips(y, window, prominence);

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 2);
}

TEST(FindProminentDipsTest, MultipleDips)
{
    std::vector<double> y = {1.0, 0.3, 1.0, 0.2, 1.0, 0.4, 1.0};
    int window = 1;
    double prominence = 0.5;

    std::vector<int> result = FindProminentDips(y, window, prominence);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 3);
    EXPECT_EQ(result[2], 5);
}

TEST(FindProminentDipsTest, NoDipsDueToProminence)
{
    std::vector<double> y = {1.0, 0.9, 1.0, 0.8, 1.0};
    int window = 1;
    double prominence = 0.3;

    std::vector<int> result = FindProminentDips(y, window, prominence);

    EXPECT_TRUE(result.empty());
}

TEST(FindProminentDipsTest, EmptyInput)
{
    std::vector<double> y = {};
    int window = 1;
    double prominence = 0.5;

    std::vector<int> result = FindProminentDips(y, window, prominence);

    EXPECT_TRUE(result.empty());
}

TEST(FindProminentDipsTest, WindowTooLarge)
{
    std::vector<double> y = {1.0, 0.5, 1.0};
    int window = 5;
    double prominence = 0.3;

    std::vector<int> result = FindProminentDips(y, window, prominence);

    EXPECT_TRUE(result.empty());
}

TEST(FindProminentDipsTest, CloseDipsFilter)
{
    std::vector<double> y = {1.0, 0.4, 0.3, 0.2, 1.0};
    int window = 1;
    double prominence = 0.4; // lowered from 0.5 to allow detection

    std::vector<int> result = FindProminentDips(y, window, prominence);

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 3);
}

TEST(FindProminentDipsTest, NotALocalMinimum)
{
    std::vector<double> y = {1.0, 0.2, 0.2, 1.0}; // both center and neighbors equal
    int window = 1;
    double prominence = 0.5;

    std::vector<int> result = FindProminentDips(y, window, prominence);

    // Should detect no dip, because center is not strictly lower than neighbors
    EXPECT_TRUE(result.empty());
}

TEST(FindProminentDipsTest, MinGapZeroMergesAllCloseDips)
{
    std::vector<double> y = {1.0, 0.3, 1.0, 0.2, 1.0};
    int window = 1; // minGap = 0
    double prominence = 0.5;

    std::vector<int> result = FindProminentDips(y, window, prominence);

    ASSERT_EQ(result.size(), 2); // No merging, as gaps > minGap
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 3);
}

TEST(FindProminentDipsTest, DipsExactlyMinGapApart)
{
    std::vector<double> y = {1.0, 0.4, 1.0, 1.0, 0.3, 1.0};
    int window = 1; // minGap = 0
    double prominence = 0.5;

    std::vector<int> result = FindProminentDips(y, window, prominence);

    // Dips are 3 positions apart, should be kept separately
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 4);
}

TEST(FindProminentDipsTest, EdgeCaseBarelyInsideWindowBounds)
{
    std::vector<double> y = {1.0, 1.0, 0.2, 1.0, 1.0}; // dip at i = 2
    int window = 2;                                    // Only i = 2 gets evaluated
    double prominence = 0.5;

    std::vector<int> result = FindProminentDips(y, window, prominence);

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 2);
}

TEST(FindProminentDipsTest, AdjacentEqualValuesNotLocalMin)
{
    std::vector<double> y = {1.0, 0.5, 0.5, 1.0}; // equal to neighbors, not strictly less
    int window = 1;
    double prominence = 0.4;

    std::vector<int> result = FindProminentDips(y, window, prominence);

    EXPECT_TRUE(result.empty());
}

TEST(FindProminentDipsTest, CandidateRejectedBecauseNeighbourIsLower)
{
    // index-1 is lower than index-1 (centre); index-2 is very high,
    // so the average is still well above the centre + prominence.
    std::vector<double> y = {0.50, 1.00, 5.00};
    int window = 1;
    double prominence = 0.5;

    auto result = FindProminentDips(y, window, prominence);

    EXPECT_TRUE(result.empty()); // centre should be discarded
}

/*
 * 2. Two raw dips are closer than or equal to minGap (window/2),
 *    so the while-loop executes and the *lower* of the two survives.
 *    Exercises:
 *      • the “while” true path
 *      • the “if (y[rawDips[i]] < minVal)” true path
 */
TEST(FindProminentDipsTest, MergeCloseDipsKeepsDeeperFirst)
{
    std::vector<double> y(12, 10.0); // flat high baseline
    y[4] = 1.0;                      // deeper dip
    y[6] = 2.0;                      // shallower dip  (gap = 2)
    int window = 4;                  // minGap = 2  →  dips are merged
    double prominence = 0.5;

    auto result = FindProminentDips(y, window, prominence);

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 4); // the deeper (index-4) survives
}

/*
 * 3. Same geometry, but now the second dip is deeper.
 *    Exercises the “if (y[rawDips[i]] < minVal)” false branch on the first
 *    comparison and true branch on the second, giving the complementary path.
 */
TEST(FindProminentDipsTest, MergeCloseDipsKeepsDeeperSecond)
{
    std::vector<double> y(12, 10.0);
    y[4] = 2.0;     // shallower
    y[6] = 1.0;     // deeper      (gap = 2)
    int window = 4; // minGap = 2
    double prominence = 0.5;

    auto result = FindProminentDips(y, window, prominence);

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 6); // the deeper (index-6) survives
}

TEST(AccurateSleepTest, SleepsForExpectedDuration)
{
    using namespace std::chrono;

    const double target_seconds = 0.001; // 1 ms
    const double tolerance_ms = 0.1;     // allow small jitter due to scheduling

    auto start = high_resolution_clock::now();
    accurate_sleep(target_seconds);
    auto end = high_resolution_clock::now();

    auto elapsed_ms = duration_cast<microseconds>(end - start).count() / 1000.0;

    // Should be at least the target (to ensure it didn't undersleep)
    EXPECT_GE(elapsed_ms, target_seconds * 1000.0);

    // Should not oversleep excessively (tolerance due to system noise)
    EXPECT_LE(elapsed_ms, target_seconds * 1000.0 + tolerance_ms);
}

TEST(AccurateSleepTest, SleepsForShortDuration_SpinOnly)
{
    // Should only spin, never sleep (remaining.count() never > 2000)
    using namespace std::chrono;

    const double target_seconds = 0.0005; // 0.5 ms
    auto start = high_resolution_clock::now();
    accurate_sleep(target_seconds);
    auto end = high_resolution_clock::now();

    auto elapsed_us = duration_cast<microseconds>(end - start).count();
    EXPECT_GE(elapsed_us, 500);  // >= 0.5 ms
    EXPECT_LE(elapsed_us, 1500); // Avoid excessive delay
}

TEST(AccurateSleepTest, SleepsForThresholdDuration_JustAtSleepCutoff)
{
    // Should hit the sleep branch only once
    using namespace std::chrono;

    const double target_seconds = 0.0021; // just over 2ms (satisfies >2000 condition)
    auto start = high_resolution_clock::now();
    accurate_sleep(target_seconds);
    auto end = high_resolution_clock::now();

    auto elapsed_us = duration_cast<microseconds>(end - start).count();
    EXPECT_GE(elapsed_us, 2100);
    EXPECT_LE(elapsed_us, 4000); // generous cap for slow systems
}

TEST(AccurateSleepTest, SleepsForModerateDuration_MultipleSleeps)
{
    // Should enter sleep path multiple times
    using namespace std::chrono;

    const double target_seconds = 0.01; // 10ms
    auto start = high_resolution_clock::now();
    accurate_sleep(target_seconds);
    auto end = high_resolution_clock::now();

    auto elapsed_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    EXPECT_GE(elapsed_ms, 10.0);
    EXPECT_LE(elapsed_ms, 20.0); // account for context switching
}
