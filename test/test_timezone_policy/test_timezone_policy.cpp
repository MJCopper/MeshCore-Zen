#include <gtest/gtest.h>

#include "../../examples/companion_radio/zen-overlay/app/zen/TimezonePolicy.h"

using zen::TimezonePolicy;

TEST(TimezonePolicy, FreshDefaultIsAutomaticSydney) {
  EXPECT_EQ(TimezonePolicy::DEFAULT_MODE, TimezonePolicy::CITY);
  EXPECT_STREQ(TimezonePolicy::cityName(TimezonePolicy::DEFAULT_CITY), "Sydney");
}

TEST(TimezonePolicy, ManualOffsetsSupportPartialHours) {
  uint32_t now = TimezonePolicy::utcDate(2026, 6, 1, 0, 0);
  EXPECT_EQ(TimezonePolicy::offsetMinutes(TimezonePolicy::MANUAL, 345, 0, now), 345);
  EXPECT_EQ(TimezonePolicy::offsetMinutes(TimezonePolicy::MANUAL, -210, 0, now), -210);
}

TEST(TimezonePolicy, AustralianCitiesApplyTheirOwnRules) {
  uint32_t january = TimezonePolicy::utcDate(2026, 1, 15, 0, 0);
  uint32_t july = TimezonePolicy::utcDate(2026, 7, 15, 0, 0);
  EXPECT_EQ(TimezonePolicy::offsetMinutes(TimezonePolicy::CITY, 0, 0, january), 660);
  EXPECT_EQ(TimezonePolicy::offsetMinutes(TimezonePolicy::CITY, 0, 0, july), 600);
  EXPECT_EQ(TimezonePolicy::offsetMinutes(TimezonePolicy::CITY, 0, 2, january), 600);
  EXPECT_EQ(TimezonePolicy::offsetMinutes(TimezonePolicy::CITY, 0, 3, january), 630);
  EXPECT_EQ(TimezonePolicy::offsetMinutes(TimezonePolicy::CITY, 0, 3, july), 570);
}

TEST(TimezonePolicy, SydneyChangesAtUtcTransitionInstants) {
  const TimezonePolicy::City& sydney = TimezonePolicy::cities()[0];
  uint32_t start = TimezonePolicy::utcDate(
      2026, 10, TimezonePolicy::nthSunday(2026, 10, 1), 2, sydney.standard_minutes);
  uint32_t end = TimezonePolicy::utcDate(
      2026, 4, TimezonePolicy::nthSunday(2026, 4, 1), 3, sydney.daylight_minutes);
  EXPECT_EQ(TimezonePolicy::offsetMinutes(TimezonePolicy::CITY, 0, 0, start - 1), 600);
  EXPECT_EQ(TimezonePolicy::offsetMinutes(TimezonePolicy::CITY, 0, 0, start), 660);
  EXPECT_EQ(TimezonePolicy::offsetMinutes(TimezonePolicy::CITY, 0, 0, end - 1), 660);
  EXPECT_EQ(TimezonePolicy::offsetMinutes(TimezonePolicy::CITY, 0, 0, end), 600);
}

TEST(TimezonePolicy, NorthernCitiesApplySeasonalRules) {
  uint32_t january = TimezonePolicy::utcDate(2026, 1, 15, 0, 0);
  uint32_t july = TimezonePolicy::utcDate(2026, 7, 15, 0, 0);
  EXPECT_EQ(TimezonePolicy::offsetMinutes(TimezonePolicy::CITY, 0, 14, january), 0);
  EXPECT_EQ(TimezonePolicy::offsetMinutes(TimezonePolicy::CITY, 0, 14, july), 60);
  EXPECT_EQ(TimezonePolicy::offsetMinutes(TimezonePolicy::CITY, 0, 16, january), -300);
  EXPECT_EQ(TimezonePolicy::offsetMinutes(TimezonePolicy::CITY, 0, 16, july), -240);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
