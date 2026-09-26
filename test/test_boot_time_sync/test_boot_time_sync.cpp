#include <gtest/gtest.h>

#include "../../examples/companion_radio/zen-overlay/app/zen/BootTimeSync.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/DeviceTimePolicy.h"

TEST(DeviceTimePolicy, AcceptsAuthoritativeBackwardCorrections) {
  EXPECT_TRUE(zen::DeviceTimePolicy::valid(1704067200UL));
  EXPECT_TRUE(zen::DeviceTimePolicy::valid(4102444799UL));
  EXPECT_FALSE(zen::DeviceTimePolicy::valid(1704067199UL));
  EXPECT_FALSE(zen::DeviceTimePolicy::valid(4102444800UL));
  EXPECT_TRUE(zen::DeviceTimePolicy::shouldPersist(2000000120UL, 2000000000UL));
}

TEST(DeviceTimePolicy, AvoidsFlashWritesForSmallCorrections) {
  EXPECT_TRUE(zen::DeviceTimePolicy::shouldPersist(0, 2000000000UL));
  EXPECT_FALSE(zen::DeviceTimePolicy::shouldPersist(2000000000UL, 1999999941UL));
  EXPECT_TRUE(zen::DeviceTimePolicy::shouldPersist(2000000000UL, 1999999940UL));
  EXPECT_FALSE(zen::DeviceTimePolicy::shouldPersist(2000000000UL, 2000000059UL));
  EXPECT_TRUE(zen::DeviceTimePolicy::shouldPersist(2000000000UL, 2000000060UL));
}

TEST(BootTimeSync, StartsTemporaryGpsAndStopsAfterAnyLiveSync) {
  zen::BootTimeSync sync;
  sync.begin(4, true, false, 1000);
  EXPECT_TRUE(sync.pending());
  EXPECT_TRUE(sync.shouldStartGps());
  EXPECT_EQ(sync.tick(5, false, true, 2000),
            zen::BootTimeSync::Action::STOP_TEMP_GPS);
  EXPECT_FALSE(sync.pending());
}

TEST(BootTimeSync, DoesNotStopGpsThatWasConfiguredOrManuallyEnabled) {
  zen::BootTimeSync configured;
  configured.begin(1, true, true, 0);
  EXPECT_FALSE(configured.shouldStartGps());
  EXPECT_EQ(configured.tick(2, true, true, 10), zen::BootTimeSync::Action::NONE);

  zen::BootTimeSync manual;
  manual.begin(1, true, false, 0);
  EXPECT_EQ(manual.tick(1, true, true, 10), zen::BootTimeSync::Action::NONE);
  EXPECT_EQ(manual.tick(2, true, true, 20), zen::BootTimeSync::Action::NONE);
}

TEST(BootTimeSync, TimesOutTemporaryGpsButKeepsWaitingForAnotherSource) {
  zen::BootTimeSync sync;
  sync.begin(7, true, false, 100);
  EXPECT_EQ(sync.tick(7, false, true, 100 + zen::BootTimeSync::GPS_TIMEOUT_MS),
            zen::BootTimeSync::Action::STOP_TEMP_GPS);
  EXPECT_TRUE(sync.pending());
  EXPECT_EQ(sync.tick(8, false, false, 100 + zen::BootTimeSync::GPS_TIMEOUT_MS + 1),
            zen::BootTimeSync::Action::NONE);
  EXPECT_FALSE(sync.pending());
}

TEST(BootTimeSync, RetriesTemporaryGpsHourlyUntilTimeIsSet) {
  zen::BootTimeSync sync;
  sync.begin(4, true, false, 100);

  EXPECT_EQ(sync.tick(4, false, true, 100 + zen::BootTimeSync::GPS_TIMEOUT_MS),
            zen::BootTimeSync::Action::STOP_TEMP_GPS);
  EXPECT_EQ(sync.tick(4, false, false,
                      100 + zen::BootTimeSync::RETRY_INTERVAL_MS - 1),
            zen::BootTimeSync::Action::NONE);
  EXPECT_EQ(sync.tick(4, false, false,
                      100 + zen::BootTimeSync::RETRY_INTERVAL_MS),
            zen::BootTimeSync::Action::START_TEMP_GPS);
  EXPECT_EQ(sync.tick(4, false, true,
                      100 + zen::BootTimeSync::RETRY_INTERVAL_MS +
                      zen::BootTimeSync::GPS_RETRY_TIMEOUT_MS),
            zen::BootTimeSync::Action::STOP_TEMP_GPS);
  EXPECT_TRUE(sync.pending());
}

TEST(BootTimeSync, StopsRetryingAfterFortyEightHours) {
  zen::BootTimeSync sync;
  sync.begin(4, true, false, 100);
  sync.tick(4, false, true, 100 + zen::BootTimeSync::GPS_TIMEOUT_MS);

  EXPECT_EQ(sync.tick(4, false, false,
                      100 + zen::BootTimeSync::RETRY_WINDOW_MS - 1),
            zen::BootTimeSync::Action::START_TEMP_GPS);
  EXPECT_TRUE(sync.pending());
  EXPECT_EQ(sync.tick(4, false, true,
                      100 + zen::BootTimeSync::RETRY_WINDOW_MS),
            zen::BootTimeSync::Action::STOP_TEMP_GPS);
  EXPECT_TRUE(sync.pending());

  // The retry window has closed: waiting longer must not restart GPS, while
  // the display-facing pending state remains until a live time source arrives.
  EXPECT_EQ(sync.tick(4, false, false,
                      100 + zen::BootTimeSync::RETRY_WINDOW_MS +
                      zen::BootTimeSync::RETRY_INTERVAL_MS),
            zen::BootTimeSync::Action::NONE);
  EXPECT_TRUE(sync.pending());

  EXPECT_EQ(sync.tick(5, false, false,
                      100 + zen::BootTimeSync::RETRY_WINDOW_MS +
                      zen::BootTimeSync::RETRY_INTERVAL_MS + 1),
            zen::BootTimeSync::Action::NONE);
  EXPECT_FALSE(sync.pending());
}

TEST(BootTimeSync, HourlyRetryStopsAfterExternalSync) {
  zen::BootTimeSync sync;
  sync.begin(12, true, false, 0);
  sync.tick(12, false, true, zen::BootTimeSync::GPS_TIMEOUT_MS);
  EXPECT_EQ(sync.tick(13, false, false, zen::BootTimeSync::RETRY_INTERVAL_MS),
            zen::BootTimeSync::Action::NONE);
  EXPECT_FALSE(sync.pending());
}

TEST(BootTimeSync, LowPowerBlocksRestartButStillObservesExternalSync) {
  zen::BootTimeSync sync;
  sync.begin(20, true, false, 0);
  EXPECT_TRUE(sync.shouldStartGps());

  // The coordinator has suspended the receiver, but the logical claim remains
  // owned. A retry deadline must not start hardware while Low Power is active.
  EXPECT_EQ(sync.tick(20, false, true, zen::BootTimeSync::GPS_TIMEOUT_MS,
                      false), zen::BootTimeSync::Action::STOP_TEMP_GPS);
  EXPECT_EQ(sync.tick(20, false, false, zen::BootTimeSync::RETRY_INTERVAL_MS,
                      false), zen::BootTimeSync::Action::NONE);

  // An app/USB time update is still authoritative while GPS retries are
  // restricted and permanently completes the pending sync.
  EXPECT_EQ(sync.tick(21, false, false,
                      zen::BootTimeSync::RETRY_INTERVAL_MS + 1, false),
            zen::BootTimeSync::Action::NONE);
  EXPECT_FALSE(sync.pending());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
