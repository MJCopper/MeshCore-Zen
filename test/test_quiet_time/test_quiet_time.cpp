#include <gtest/gtest.h>
#include <Stream.h>

#include "../../examples/companion_radio/zen-overlay/app/ui-new/QuietTimePolicy.h"
#include "../../examples/companion_radio/zen-overlay/app/ui-new/QuietTime.h"

TEST(QuietTime, RequiresLiveSyncEvenWithPlausibleRestoredTimestamp) {
  ZenPrefs prefs = {};
  prefs.quiet_time_enabled = 1;
  prefs.quiet_time_start_min = 21 * 60;
  prefs.quiet_time_end_min = 7 * 60;
  prefs.timezone_mode = zen::TimezonePolicy::MANUAL;
  prefs.timezone_manual_min = 10 * 60;
  const uint32_t utc = 20000UL * 86400 + 12 * 3600; // 22:00 local
  EXPECT_FALSE(quiettime::active(&prefs, utc, false));
  EXPECT_TRUE(quiettime::active(&prefs, utc, true));
  EXPECT_FALSE(quiettime::active(&prefs, 0, true));
  prefs.quiet_time_enabled = 0;
  EXPECT_FALSE(quiettime::active(&prefs, utc, true));
}

TEST(QuietTime, HandlesSameDayIntervalBoundaries) {
  EXPECT_FALSE(quiettime::intervalActive(8 * 60 + 59, 9 * 60, 17 * 60));
  EXPECT_TRUE(quiettime::intervalActive(9 * 60, 9 * 60, 17 * 60));
  EXPECT_TRUE(quiettime::intervalActive(16 * 60 + 59, 9 * 60, 17 * 60));
  EXPECT_FALSE(quiettime::intervalActive(17 * 60, 9 * 60, 17 * 60));
}

TEST(QuietTime, HandlesIntervalAcrossMidnight) {
  EXPECT_TRUE(quiettime::intervalActive(23 * 60, 21 * 60, 7 * 60));
  EXPECT_TRUE(quiettime::intervalActive(6 * 60 + 59, 21 * 60, 7 * 60));
  EXPECT_FALSE(quiettime::intervalActive(12 * 60, 21 * 60, 7 * 60));
}

TEST(QuietTime, RejectsDisabledAndInvalidIntervals) {
  EXPECT_FALSE(quiettime::intervalActive(12 * 60, 7 * 60, 7 * 60));
  EXPECT_FALSE(quiettime::intervalActive(1440, 9 * 60, 17 * 60));
  EXPECT_FALSE(quiettime::intervalActive(12 * 60, 1440, 17 * 60));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
