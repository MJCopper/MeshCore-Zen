#include <gtest/gtest.h>

#include "../../examples/companion_radio/zen-overlay/app/zen/GpsMode.h"
#include "../../examples/companion_radio/zen-overlay/src/helpers/sensors/GpsPollingPolicy.h"

TEST(GpsMode, ExposesRequestedModesAndIntervals) {
  EXPECT_EQ(zen::GpsMode::COUNT, 8);
  EXPECT_STREQ(zen::GpsMode::label(0), "Off");
  EXPECT_STREQ(zen::GpsMode::label(1), "Continuous");
  EXPECT_STREQ(zen::GpsMode::label(2), "Adaptive");
  EXPECT_EQ(zen::GpsMode::interval(2), 0U);
  EXPECT_EQ(zen::GpsMode::interval(3), 120U);
  EXPECT_EQ(zen::GpsMode::interval(4), 300U);
  EXPECT_EQ(zen::GpsMode::interval(5), 900U);
  EXPECT_EQ(zen::GpsMode::interval(6), 1800U);
  EXPECT_EQ(zen::GpsMode::interval(7), 3600U);
}

TEST(GpsMode, MapsStoredPreferencesBackToMenuModes) {
  EXPECT_EQ(zen::GpsMode::fromPrefs(false, 3600), 0);
  EXPECT_EQ(zen::GpsMode::fromPrefs(true, 0), 1);
  EXPECT_EQ(zen::GpsMode::fromPrefs(true, 0, true), 2);
  EXPECT_EQ(zen::GpsMode::fromPrefs(true, 900), 5);
  EXPECT_EQ(zen::GpsMode::fromPrefs(true, 1000), 5);
}

TEST(GpsMode, SeparatesPollingChoicesFromPower) {
  EXPECT_EQ(zen::GpsMode::POLLING_COUNT, 7);
  EXPECT_STREQ(zen::GpsMode::pollingLabel(0), "Continuous");
  EXPECT_STREQ(zen::GpsMode::pollingLabel(1), "Adaptive");
  EXPECT_STREQ(zen::GpsMode::pollingLabel(2), "2mins");
  EXPECT_EQ(zen::GpsMode::pollingInterval(0), 0U);
  EXPECT_EQ(zen::GpsMode::pollingInterval(1), 0U);
  EXPECT_EQ(zen::GpsMode::pollingInterval(6), 3600U);
  EXPECT_EQ(zen::GpsMode::pollingFromPrefs(900, false), 4);
  EXPECT_EQ(zen::GpsMode::pollingFromPrefs(0, true), 1);
  EXPECT_TRUE(zen::GpsMode::pollingIsAdaptive(1));
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
  EXPECT_EQ(GpsPollingPolicy::retryDelaySeconds(300, 3), 600U);
  EXPECT_EQ(GpsPollingPolicy::retryDelaySeconds(300, 255), 600U);
  EXPECT_EQ(GpsPollingPolicy::retryDelaySeconds(3600, 3), 7200U);
  EXPECT_EQ(GpsPollingPolicy::retryDelaySeconds(10800, 3), 7200U);
}

TEST(GpsPollingPolicy, RetriesFailedTimedPollingOnUserWake) {
  EXPECT_TRUE(GpsPollingPolicy::retryOnUserWake(true, false, 300, 1));
  EXPECT_TRUE(GpsPollingPolicy::retryOnUserWake(true, false, 300, 3));
  EXPECT_FALSE(GpsPollingPolicy::retryOnUserWake(true, false, 300, 0));
  EXPECT_FALSE(GpsPollingPolicy::retryOnUserWake(false, false, 300, 3));
  EXPECT_FALSE(GpsPollingPolicy::retryOnUserWake(true, true, 300, 3));
  EXPECT_FALSE(GpsPollingPolicy::retryOnUserWake(true, false, 0, 3));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
