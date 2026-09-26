#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/SensorAccessCoordinator.h"

TEST(SensorAccess, DefersTelemetryAfterSuccessfulBlankLogin) {
  zen::SensorAccessCoordinator flow;
  flow.telemetryStarted();
  EXPECT_EQ(flow.telemetryTimedOut(), zen::SensorAccessCoordinator::START_BLANK_LOGIN);
  flow.loginStarted(false);
  flow.loginSucceeded(100);
  EXPECT_TRUE(flow.accessConfirmed());
  EXPECT_EQ(flow.tick(1099), zen::SensorAccessCoordinator::NONE);
  EXPECT_EQ(flow.tick(1100), zen::SensorAccessCoordinator::SEND_TELEMETRY);
}

TEST(SensorAccess, DoesNotRepeatBlankLoginAfterAccessIsConfirmed) {
  zen::SensorAccessCoordinator flow;
  flow.loginStarted(false);
  flow.loginSucceeded(UINT32_MAX - 500);
  EXPECT_EQ(flow.tick(499), zen::SensorAccessCoordinator::SEND_TELEMETRY);
  flow.telemetryStarted();
  EXPECT_EQ(flow.telemetryTimedOut(), zen::SensorAccessCoordinator::NONE);
  EXPECT_EQ(flow.phase(), zen::SensorAccessCoordinator::ERROR);
}

TEST(SensorAccess, IgnoresStaleTelemetryFailureDuringLoginAndRetryDelay) {
  zen::SensorAccessCoordinator flow;
  flow.telemetryStarted();
  ASSERT_EQ(flow.telemetryTimedOut(), zen::SensorAccessCoordinator::START_BLANK_LOGIN);
  EXPECT_EQ(flow.telemetryTimedOut(), zen::SensorAccessCoordinator::NONE);
  EXPECT_EQ(flow.phase(), zen::SensorAccessCoordinator::ACL_WAIT);
  flow.loginSucceeded(100);
  EXPECT_EQ(flow.telemetryTimedOut(), zen::SensorAccessCoordinator::NONE);
  EXPECT_EQ(flow.phase(), zen::SensorAccessCoordinator::RETRY_DELAY);
}

TEST(SensorAccess, OffersPasswordOnlyAfterBlankLoginFails) {
  zen::SensorAccessCoordinator flow;
  flow.telemetryStarted();
  ASSERT_EQ(flow.telemetryTimedOut(), zen::SensorAccessCoordinator::START_BLANK_LOGIN);
  flow.loginStarted(false);
  flow.loginFailed(false);
  EXPECT_EQ(flow.phase(), zen::SensorAccessCoordinator::LOGIN_OFFER);
  flow.offerPassword();
  EXPECT_TRUE(flow.passwordEditing());
}

TEST(SensorAccess, KnownPathRetriesThenFallsBackToThreeFloods) {
  zen::SensorAccessCoordinator flow;
  flow.beginTelemetry(true);
  EXPECT_EQ(flow.telemetryTimedOut(zen::RemoteNodeOperation::RETRY_PATH),
            zen::SensorAccessCoordinator::SEND_TELEMETRY);
  flow.telemetryStarted();
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(flow.telemetryTimedOut(zen::RemoteNodeOperation::RETRY_FLOOD),
              zen::SensorAccessCoordinator::CLEAR_PATH_AND_SEND_TELEMETRY);
    flow.telemetryStarted();
  }
  EXPECT_EQ(flow.telemetryTimedOut(zen::RemoteNodeOperation::FINISH_TIMEOUT),
            zen::SensorAccessCoordinator::START_BLANK_LOGIN);
}

TEST(SensorAccess, UnknownPathGetsThreeFloodTriesTotal) {
  zen::SensorAccessCoordinator flow;
  flow.beginTelemetry(false);
  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(flow.telemetryTimedOut(zen::RemoteNodeOperation::RETRY_FLOOD),
              zen::SensorAccessCoordinator::CLEAR_PATH_AND_SEND_TELEMETRY);
    flow.telemetryStarted();
  }
  EXPECT_EQ(flow.telemetryTimedOut(zen::RemoteNodeOperation::FINISH_TIMEOUT),
            zen::SensorAccessCoordinator::START_BLANK_LOGIN);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
