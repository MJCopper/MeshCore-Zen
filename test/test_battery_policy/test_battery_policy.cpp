#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/BatteryPolicy.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/BatteryRuntime.h"

TEST(BatteryPolicy, FixedCutoffHonoursExternalPowerAndInvalidReading) {
  EXPECT_TRUE(zen::BatteryPolicy::shouldShutdown(3300, false));
  EXPECT_TRUE(zen::BatteryPolicy::shouldShutdown(3299, false));
  EXPECT_FALSE(zen::BatteryPolicy::shouldShutdown(3301, false));
  EXPECT_FALSE(zen::BatteryPolicy::shouldShutdown(3200, true));
  EXPECT_FALSE(zen::BatteryPolicy::shouldShutdown(0, false));
}

TEST(BatteryPolicy, PercentageEndpointsAndBounds) {
  EXPECT_EQ(zen::BatteryPolicy::percent(3300), 0);
  EXPECT_LT(zen::BatteryPolicy::percent(4119), 100);
  EXPECT_EQ(zen::BatteryPolicy::percent(4120), 100);
  EXPECT_EQ(zen::BatteryPolicy::percent(4200), 100);
  EXPECT_EQ(zen::BatteryPolicy::percent(0), 0);
  EXPECT_EQ(zen::BatteryPolicy::percent(5000), 100);
  EXPECT_EQ(zen::BatteryPolicy::percent(4199), 100);
  EXPECT_EQ(zen::BatteryPolicy::percent(4170), 100);
  EXPECT_EQ(zen::BatteryPolicy::percent(3500), 5);
  EXPECT_EQ(zen::BatteryPolicy::percent(3700), 30);
  EXPECT_EQ(zen::BatteryPolicy::percent(3800), 55);
  EXPECT_EQ(zen::BatteryPolicy::percent(4000), 85);
  EXPECT_EQ(zen::BatteryPolicy::percentX100(3750), 4200);
  EXPECT_GT(zen::BatteryPolicy::percentX100(3751), 4200);
}

TEST(BatteryRuntime, StartsWithFiveDayModel) {
  zen::BatteryRuntimeEstimator estimate;
  estimate.update(0, 4120, false, false);
  EXPECT_EQ(estimate.state(), zen::BatteryRuntimeEstimator::MODEL);
  EXPECT_EQ(estimate.seconds(), 5UL * 24UL * 60UL * 60UL);
  estimate.update(1000, 4000, false, false);
  EXPECT_EQ(estimate.seconds(), 102UL * 60UL * 60UL); // 85% of five days
}

TEST(BatteryRuntime, LearnsOnlyAfterSixHoursAndFivePercentDischarge) {
  zen::BatteryRuntimeEstimator estimate;
  estimate.update(0, 4000, false, false);
  for (uint32_t hour = 1; hour < 6; hour++)
    estimate.update(hour * 3600000UL, 4000 - hour * 6, false, false);
  EXPECT_EQ(estimate.state(), zen::BatteryRuntimeEstimator::MODEL);
  estimate.update(6UL * 3600000UL, 3960, false, false);
  EXPECT_EQ(estimate.state(), zen::BatteryRuntimeEstimator::ESTIMATE);
  EXPECT_GT(estimate.confidencePermille(), 0);
  EXPECT_GT(estimate.seconds(), 0u);
}

TEST(BatteryRuntime, FlatVoltageFallsBackToModel) {
  zen::BatteryRuntimeEstimator estimate;
  for (uint32_t hour = 0; hour <= 12; hour++)
    estimate.update(hour * 3600000UL, 3900, false, false);
  EXPECT_EQ(estimate.state(), zen::BatteryRuntimeEstimator::MODEL);
  EXPECT_EQ(estimate.seconds(), 5UL * 24UL * 60UL * 60UL * 72UL / 100UL);
}

TEST(BatteryRuntime, ResetsForChargingEmergencyAndVoltageRecovery) {
  zen::BatteryRuntimeEstimator estimate;
  estimate.update(0, 3900, false, false);
  estimate.update(60UL * 60000UL, 3880, false, false);
  estimate.update(61UL * 60000UL, 3880, true, false);
  EXPECT_EQ(estimate.state(), zen::BatteryRuntimeEstimator::CHARGING);
  EXPECT_EQ(estimate.sampleCount(), 0);
  estimate.update(62UL * 60000UL, 3880, false, false);
  EXPECT_EQ(estimate.state(), zen::BatteryRuntimeEstimator::MODEL);
  EXPECT_EQ(estimate.sampleCount(), 0); // two-hour post-charge settling window
  estimate.update(181UL * 60000UL, 3840, false, false);
  EXPECT_EQ(estimate.sampleCount(), 0);
  estimate.update(182UL * 60000UL, 3840, false, false);
  EXPECT_EQ(estimate.sampleCount(), 1);
  estimate.update(183UL * 60000UL, 3840, false, true);
  EXPECT_EQ(estimate.state(), zen::BatteryRuntimeEstimator::PAUSED);
  estimate.update(184UL * 60000UL, 3800, false, false);
  estimate.update(244UL * 60000UL, 3890, false, false);
  EXPECT_EQ(estimate.state(), zen::BatteryRuntimeEstimator::MODEL);
  EXPECT_EQ(estimate.sampleCount(), 1);
}

TEST(BatteryRuntime, EndsAtThreePointThreeVoltsAndHandlesMillisWrap) {
  zen::BatteryRuntimeEstimator estimate;
  estimate.update(0xFFF00000UL, 3800, false, false);
  estimate.update((uint32_t)(0xFFF00000UL + 60UL * 60000UL), 3790, false, false);
  EXPECT_EQ(estimate.sampleCount(), 2);
  estimate.update(0, 3300, false, false);
  EXPECT_EQ(estimate.state(), zen::BatteryRuntimeEstimator::EMPTY);
}

TEST(BatteryPolicy, CurveIsMonotonicAndNonlinear) {
  int previous = 0;
  for (int mv = 3000; mv <= 4400; mv++) {
    int percent = zen::BatteryPolicy::percent(mv);
    EXPECT_GE(percent, previous);
    EXPECT_LE(percent, 100);
    previous = percent;
  }
  EXPECT_EQ(zen::BatteryPolicy::percent(3700), 30);
  EXPECT_EQ(zen::BatteryPolicy::percent(3800), 55);
}

TEST(LowBatteryReminder, FirstLowSampleThenHourlyIncludingThresholdChatter) {
  zen::LowBatteryReminder reminder;
  EXPECT_FALSE(reminder.due(0, 3700, false));
  EXPECT_TRUE(reminder.due(8000, 3500, false));
  EXPECT_FALSE(reminder.due(16000, 3700, false));
  EXPECT_FALSE(reminder.due(24000, 3500, false));
  EXPECT_FALSE(reminder.due(3607999, 3500, false));
  EXPECT_TRUE(reminder.due(3608000, 3500, false));
}

TEST(LowBatteryReminder, IgnoresChargingInvalidAndShutdownSamples) {
  zen::LowBatteryReminder reminder;
  EXPECT_FALSE(reminder.due(0, 3500, true));
  EXPECT_FALSE(reminder.due(0, 0, false));
  EXPECT_FALSE(reminder.due(0, 3300, false));
  EXPECT_TRUE(reminder.due(0, 3500, false));
  EXPECT_FALSE(reminder.due(3600000, 3500, true));
  EXPECT_TRUE(reminder.due(3608000, 3500, false));
  EXPECT_FALSE(reminder.due(3609000, 3500, true));
  EXPECT_TRUE(reminder.due(3610000, 3500, false));
}

TEST(LowPowerLatch, LatchesUntilExternalPower) {
  zen::LowPowerLatch latch;
  EXPECT_FALSE(latch.update(3700, false));
  int mv = 3301;
  while (zen::BatteryPolicy::percent(mv) <= 5) mv++;
  EXPECT_FALSE(latch.update(mv - 1, false));
  EXPECT_FALSE(latch.update(mv - 1, false));
  EXPECT_TRUE(latch.update(mv - 1, false));
  EXPECT_TRUE(latch.update(4000, false));
  EXPECT_FALSE(latch.update(4000, true));
  EXPECT_FALSE(latch.update(4000, false));
}

TEST(LowPowerLatch, RequiresThreeConsecutiveLowSamples) {
  zen::LowPowerLatch latch;
  EXPECT_FALSE(latch.update(3500, false));
  EXPECT_FALSE(latch.update(3500, false));
  EXPECT_FALSE(latch.update(3600, false));
  EXPECT_FALSE(latch.update(3500, false));
  EXPECT_FALSE(latch.update(3500, false));
  EXPECT_TRUE(latch.update(3500, false));
}

TEST(LowPowerLatch, DoesNotReplaceShutdownOrRunWithoutReading) {
  zen::LowPowerLatch latch;
  EXPECT_FALSE(latch.update(0, false));
  EXPECT_FALSE(latch.update(3100, false));
  EXPECT_FALSE(latch.update(3000, false));
  EXPECT_FALSE(latch.update(3500, true));
}

TEST(EmergencyWindow, RunsForTenMinutesAndHandlesRollover) {
  zen::EmergencyWindow window;
  EXPECT_FALSE(window.active());
  window.start(0xFFFFFF00UL);
  EXPECT_TRUE(window.active());
  EXPECT_EQ(window.remainingSeconds(0xFFFFFF00UL), 600u);
  EXPECT_TRUE(window.update((uint32_t)(0xFFFFFF00UL + 599999UL)));
  EXPECT_EQ(window.remainingSeconds((uint32_t)(0xFFFFFF00UL + 599999UL)), 1u);
  EXPECT_FALSE(window.update((uint32_t)(0xFFFFFF00UL + 600000UL)));
  EXPECT_EQ(window.remainingSeconds(0), 0u);
}

TEST(EmergencyWindow, CanBeCancelledWithoutPersistence) {
  zen::EmergencyWindow window;
  window.start(1000);
  window.cancel();
  EXPECT_FALSE(window.active());
  EXPECT_FALSE(window.update(2000));
}

TEST(LowBatteryReminder, HandlesMillisRolloverAndTwentyPercentBoundary) {
  zen::LowBatteryReminder reminder;
  int mv = 3301;
  while (zen::BatteryPolicy::percent(mv + 1) <= 20) mv++;
  EXPECT_FALSE(reminder.due(0xFFFFFF00UL, mv + 1, false));
  EXPECT_TRUE(reminder.due(0xFFFFFF00UL, mv, false));
  EXPECT_FALSE(reminder.due(1000, mv, false));
  EXPECT_TRUE(reminder.due((uint32_t)(0xFFFFFF00UL + 3600000UL), mv, false));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
