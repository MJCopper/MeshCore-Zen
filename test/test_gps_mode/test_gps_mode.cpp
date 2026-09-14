#include <gtest/gtest.h>

#include "../../examples/companion_radio/solo/GpsMode.h"
#include "../../src/helpers/sensors/GpsPollingPolicy.h"

TEST(GpsMode, ExposesRequestedModesAndIntervals) {
  EXPECT_EQ(solo::GpsMode::COUNT, 9);
  EXPECT_STREQ(solo::GpsMode::label(0), "Off");
  EXPECT_STREQ(solo::GpsMode::label(1), "Continuous");
  EXPECT_EQ(solo::GpsMode::interval(2), 120U);
  EXPECT_EQ(solo::GpsMode::interval(3), 300U);
  EXPECT_EQ(solo::GpsMode::interval(4), 900U);
  EXPECT_EQ(solo::GpsMode::interval(5), 1800U);
  EXPECT_EQ(solo::GpsMode::interval(6), 3600U);
  EXPECT_EQ(solo::GpsMode::interval(7), 10800U);
  EXPECT_EQ(solo::GpsMode::interval(8), 21600U);
}

TEST(GpsMode, MapsStoredPreferencesBackToMenuModes) {
  EXPECT_EQ(solo::GpsMode::fromPrefs(false, 3600), 0);
  EXPECT_EQ(solo::GpsMode::fromPrefs(true, 0), 1);
  EXPECT_EQ(solo::GpsMode::fromPrefs(true, 900), 4);
  EXPECT_EQ(solo::GpsMode::fromPrefs(true, 1000), 4);
}

TEST(GpsMode, SeparatesPollingChoicesFromPower) {
  EXPECT_EQ(solo::GpsMode::POLLING_COUNT, 8);
  EXPECT_STREQ(solo::GpsMode::pollingLabel(0), "Continuous");
  EXPECT_STREQ(solo::GpsMode::pollingLabel(1), "2mins");
  EXPECT_EQ(solo::GpsMode::pollingInterval(0), 0U);
  EXPECT_EQ(solo::GpsMode::pollingInterval(7), 21600U);
  EXPECT_EQ(solo::GpsMode::pollingFromInterval(900), 3);
}

TEST(GpsPollingPolicy, UsesHdopOrSatelliteFallback) {
  EXPECT_TRUE(GpsPollingPolicy::qualityGood(20, 0));
  EXPECT_TRUE(GpsPollingPolicy::qualityGood(40, 0));
  EXPECT_FALSE(GpsPollingPolicy::qualityGood(41, 20));
  EXPECT_TRUE(GpsPollingPolicy::qualityGood(-1, 8));
  EXPECT_FALSE(GpsPollingPolicy::qualityGood(-1, 7));
}

TEST(GpsPollingPolicy, BacksOffFailuresWithoutChangingConfiguredInterval) {
  EXPECT_EQ(GpsPollingPolicy::retryDelaySeconds(300, 0), 300U);
  EXPECT_EQ(GpsPollingPolicy::retryDelaySeconds(300, 1), 300U);
  EXPECT_EQ(GpsPollingPolicy::retryDelaySeconds(300, 2), 600U);
  EXPECT_EQ(GpsPollingPolicy::retryDelaySeconds(300, 3), 1200U);
  EXPECT_EQ(GpsPollingPolicy::retryDelaySeconds(10800, 3), 21600U);
  EXPECT_EQ(GpsPollingPolicy::retryDelaySeconds(21600, 3), 21600U);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
