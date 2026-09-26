#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/RepeaterSignalMonitor.h"

TEST(RepeaterSignal, AcceptsRelayedTrafficButNotDirectCompanions) {
  EXPECT_FALSE(zen::RepeaterSignalMonitor::qualifiesRoute(true, false, 0));
  EXPECT_TRUE(zen::RepeaterSignalMonitor::qualifiesRoute(true, false, 1));
  EXPECT_FALSE(zen::RepeaterSignalMonitor::qualifiesRoute(false, true, 1));
  EXPECT_TRUE(zen::RepeaterSignalMonitor::qualifiesRoute(false, true, 2));
  EXPECT_FALSE(zen::RepeaterSignalMonitor::qualifiesRoute(false, false, 4));
}

TEST(RepeaterSignal, ClassifiesLinkMarginAndAveragesSamples) {
  zen::RepeaterSignalMonitor signal;
  signal.noteSample(-25, 100); // -6.25 dB at SF7: 1.25 dB margin
  EXPECT_EQ(signal.bars(7, 100, true), 1);
  signal.noteSample(15, 200);  // average -1.25 dB: 6.25 dB margin
  EXPECT_EQ(signal.bars(7, 200, true), 2);
  signal.noteSample(55, 300);  // average 3.75 dB: 11.25 dB margin
  EXPECT_EQ(signal.bars(7, 300, true), 3);
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
