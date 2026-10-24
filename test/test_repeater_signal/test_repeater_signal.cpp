#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/RepeaterSignalMonitor.h"

TEST(RepeaterSignal, AcceptsRelayedTrafficButNotDirectCompanions) {
  EXPECT_FALSE(zen::RepeaterSignalMonitor::qualifiesRoute(true, false, 0));
  EXPECT_TRUE(zen::RepeaterSignalMonitor::qualifiesRoute(true, false, 1));
  EXPECT_FALSE(zen::RepeaterSignalMonitor::qualifiesRoute(false, true, 1));
  EXPECT_TRUE(zen::RepeaterSignalMonitor::qualifiesRoute(false, true, 2));
  EXPECT_FALSE(zen::RepeaterSignalMonitor::qualifiesRoute(false, false, 4));
}

TEST(RepeaterSignal, ExactBoundariesAcrossAllSpreadingFactors) {
  const int sf7_snr[] = {-120, 0, 20, 21, 28, 29, 39, 40, 80};
  const int expected[] = {1, 1, 1, 2, 2, 3, 3, 4, 4};
  for (int sf = 5; sf <= 12; ++sf) {
    for (unsigned i = 0; i < sizeof(expected)/sizeof(*expected); ++i) {
      zen::RepeaterSignalMonitor signal;
      signal.noteSample(sf7_snr[i] - 10 * (sf - 7), 100);
      EXPECT_EQ(signal.bars(sf, 100, true), expected[i]) << "SF=" << sf << " sample=" << i;
    }
  }
}

TEST(RepeaterSignal, AveragesWithoutRoundingAwayThresholdCrossings) {
  for (int sf = 5; sf <= 12; ++sf) {
    for (int boundary : {20, 28, 40}) {
      zen::RepeaterSignalMonitor signal;
      const int value = boundary - 10 * (sf - 7);
      signal.noteSample(value, 100);
      signal.noteSample(value, 101);
      signal.noteSample(value, 102);
      signal.noteSample(value + (boundary == 40 ? -1 : 1), 103);
      EXPECT_EQ(signal.bars(sf, 103, true), boundary == 20 ? 2 : 3);
      // Replacing every slot with the boundary must recover the exact level.
      for (int i = 0; i < 4; ++i) signal.noteSample(value, 104 + i);
      EXPECT_EQ(signal.averageSnrX4(), value);
      EXPECT_EQ(signal.bars(sf, 108, true), boundary == 20 ? 1 : boundary == 28 ? 2 : 4);
    }
  }
}

TEST(RepeaterSignal, ConfigurationChangesClearSamplesScansAndDiscovery) {
  zen::RepeaterSignalMonitor signal;
  EXPECT_TRUE(signal.configure(916.575f, 62.5f, 7, 8));
  auto seed = [&]() {
    signal.noteSample(40, 100);
    signal.noteScanAttempt(100);
    signal.beginDiscovery();
    signal.noteDiscovery(60);
  };
  seed();
  EXPECT_FALSE(signal.configure(916.575f, 62.5f, 7, 8));
  EXPECT_EQ(signal.bars(7, 101, true), 4);
  const float configs[][4] = {
    {917.0f, 62.5f, 7, 8}, {917.0f, 125, 7, 8},
    {917.0f, 125, 8, 8}, {917.0f, 125, 8, 5}
  };
  for (const auto& c : configs) {
    EXPECT_TRUE(signal.configure(c[0], c[1], c[2], c[3]));
    signal.finishDiscovery(200);
    EXPECT_EQ(signal.bars(c[2], 200, true), 0);
    EXPECT_EQ(signal.averageSnrX4(), INT16_MIN);
    EXPECT_TRUE(signal.shouldScanOnUserWake(200, true));
    seed();
  }
}

TEST(RepeaterSignal, EmptyInvalidAndClampedSamples) {
  zen::RepeaterSignalMonitor signal;
  EXPECT_EQ(signal.bars(7, 100, true), 0);
  signal.noteSample(1000, 100);
  EXPECT_EQ(signal.averageSnrX4(), INT8_MAX);
  EXPECT_EQ(signal.bars(4, 100, true), 0);
  EXPECT_EQ(signal.bars(13, 100, true), 0);
  signal.reset();
  signal.noteSample(-1000, 100);
  EXPECT_EQ(signal.averageSnrX4(), INT8_MIN);
  EXPECT_EQ(signal.bars(7, 100, true), 1);
}

TEST(RepeaterSignal, ScansOnlyOnStaleUserWakeAndRateLimitsFailures) {
  zen::RepeaterSignalMonitor signal;
  EXPECT_TRUE(signal.shouldScanOnUserWake(10, true));
  signal.noteScanAttempt(10);
  EXPECT_FALSE(signal.shouldScanOnUserWake(1000, true));
  EXPECT_TRUE(signal.shouldScanOnUserWake(1800010, true));
  signal.noteSample(0, 1800010);
  EXPECT_FALSE(signal.shouldScanOnUserWake(3600009, true));
  EXPECT_TRUE(signal.shouldScanOnUserWake(3600010, true));
  EXPECT_FALSE(signal.shouldScanOnUserWake(3600010, false));
}

TEST(RepeaterSignal, DiscoveryResultIsAuthoritative) {
  zen::RepeaterSignalMonitor signal;
  signal.noteSample(40, 100);
  signal.beginDiscovery();
  signal.noteDiscovery(-30);
  signal.noteDiscovery(10);
  signal.finishDiscovery(200);
  EXPECT_EQ(signal.averageSnrX4(), 10);

  signal.beginDiscovery();
  signal.finishDiscovery(300);
  EXPECT_EQ(signal.bars(10, 300, true), 0);
}

TEST(RepeaterSignal, ExpiresAndHandlesMillisRollover) {
  zen::RepeaterSignalMonitor signal;
  signal.noteSample(0, UINT32_MAX - 1000);
  EXPECT_NE(signal.bars(10, 500, true), 0);
  EXPECT_EQ(signal.bars(10, 7200000 - 999, true), 0);
  EXPECT_EQ(signal.bars(10, 500, false), 0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
