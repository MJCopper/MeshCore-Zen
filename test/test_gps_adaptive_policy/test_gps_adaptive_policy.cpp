#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/src/helpers/sensors/GpsAdaptivePolicy.h"

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

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
