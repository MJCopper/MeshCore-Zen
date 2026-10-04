#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/src/helpers/sensors/GpsAdaptivePolicy.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/GpsFreshness.h"

TEST(GpsAdaptivePolicy, InitialFixTracksThenLostFixBacksOff) {
  GpsAdaptivePolicy policy;
  policy.setEnabled(1000, true, false);
  EXPECT_EQ(GpsAdaptivePolicy::START, policy.update(1000, false, false));
  EXPECT_EQ(GpsAdaptivePolicy::KEEP, policy.update(2000, true, true));
  EXPECT_EQ(GpsAdaptivePolicy::TRACKING, policy.phase());
  policy.update(3000, false, true);
  EXPECT_EQ(GpsAdaptivePolicy::KEEP, policy.update(302999, false, true));
  EXPECT_EQ(GpsAdaptivePolicy::STOP, policy.update(303000, false, true));
  EXPECT_EQ(GpsAdaptivePolicy::SHORT_STANDBY, policy.phase());
}

TEST(GpsAdaptivePolicy, ShortThenLongBackoff) {
  GpsAdaptivePolicy policy;
  policy.setEnabled(1000, true, true);
  EXPECT_EQ(GpsAdaptivePolicy::STOP, policy.update(301000, false, true));
  EXPECT_EQ(GpsAdaptivePolicy::START, policy.update(421000, false, false));
  EXPECT_EQ(GpsAdaptivePolicy::STOP, policy.update(511000, false, true));
  EXPECT_EQ(GpsAdaptivePolicy::SHORT_STANDBY, policy.phase());

  EXPECT_EQ(GpsAdaptivePolicy::START, policy.update(1201000, false, false));
  EXPECT_EQ(GpsAdaptivePolicy::STOP, policy.update(1291000, false, true));
  EXPECT_EQ(GpsAdaptivePolicy::LONG_STANDBY, policy.phase());
  uint32_t remaining = 0;
  EXPECT_TRUE(policy.retryRemaining(1291000, remaining));
  EXPECT_EQ(300000U, remaining);
}

TEST(GpsAdaptivePolicy, UserWakeResetsBackoffToFullSearch) {
  GpsAdaptivePolicy policy;
  policy.setEnabled(1000, true, true);
  policy.update(301000, false, true);
  EXPECT_EQ(GpsAdaptivePolicy::START, policy.onUserWake(302000, false));
  EXPECT_EQ(GpsAdaptivePolicy::INITIAL_SEARCH, policy.phase());
  EXPECT_EQ(GpsAdaptivePolicy::KEEP, policy.update(601999, false, true));
  EXPECT_EQ(GpsAdaptivePolicy::STOP, policy.update(602000, false, true));
}

TEST(GpsAdaptivePolicy, ForcedPowerBypassesBackoff) {
  GpsAdaptivePolicy policy;
  policy.setEnabled(1000, true, true);
  policy.update(301000, false, true);
  EXPECT_EQ(GpsAdaptivePolicy::START,
            policy.update(302000, false, false, true));
  EXPECT_EQ(GpsAdaptivePolicy::KEEP,
            policy.update(303000, true, true, true));
  EXPECT_EQ(GpsAdaptivePolicy::TRACKING, policy.phase());
}

TEST(GpsAdaptivePolicy, ClaimReleaseRestartsFullAcquisition) {
  GpsAdaptivePolicy policy;
  policy.setEnabled(1000, true, true);
  policy.update(400000, false, true, true);
  policy.restart(400000);
  EXPECT_EQ(GpsAdaptivePolicy::KEEP, policy.update(699999, false, true));
  EXPECT_EQ(GpsAdaptivePolicy::STOP, policy.update(700000, false, true));
  policy.setEnabled(700001, false, false);
  policy.setEnabled(800000, true, false);
  EXPECT_EQ(GpsAdaptivePolicy::START, policy.update(800000, false, false));
}

TEST(GpsFreshness, ExpiresCachedFixAndHandlesWrapAndReset) {
  zen::GpsFreshness fix;
  EXPECT_FALSE(fix.valid(0));
  fix.record(0xfffffff0UL, true);
  EXPECT_TRUE(fix.valid(20));
  EXPECT_EQ(1U, fix.sequence());
  EXPECT_FALSE(fix.valid(5000));
  fix.record(6000, true);
  fix.reset();
  EXPECT_FALSE(fix.valid(6001));
  fix.record(7000, true);
  fix.record(7001, false);
  EXPECT_FALSE(fix.valid(7002));
  EXPECT_EQ(3U, fix.sequence());
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
