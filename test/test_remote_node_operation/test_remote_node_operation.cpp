#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/RemoteNodeOperation.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/RemoteNodeCoordinator.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/RemoteAdminAdapter.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/RemoteTelemetryAdapter.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/RemoteLoginAdapter.h"

TEST(RemoteNodeOperation, KnownPathRetriesOnceThenFloodsThreeTimes) {
  zen::RemoteNodeOperation op;
  uint8_t key[32] = {1};
  op.begin(op.TELEMETRY, key, true, op.RETRY_SAFE, 10);
  EXPECT_EQ(op.route(), op.PATH);
  EXPECT_EQ(op.attempt(), 1);
  EXPECT_EQ(op.onTimeout(9), op.WAIT);
  EXPECT_EQ(op.onTimeout(10), op.RETRY_PATH);
  ASSERT_TRUE(op.restart(op.PATH, 20));
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(op.onTimeout(20 + i), op.RETRY_FLOOD);
    ASSERT_TRUE(op.restart(op.FLOOD, 21 + i));
  }
  EXPECT_EQ(op.onTimeout(23), op.FINISH_TIMEOUT);
  EXPECT_EQ(op.result(), op.RETRIES_EXHAUSTED);
  EXPECT_EQ(op.attempt(), 5);
}

TEST(RemoteNodeOperation, UnknownPathHasThreeFloodAttemptsTotal) {
  zen::RemoteNodeOperation op;
  op.begin(op.LOGIN, nullptr, false, op.RETRY_SAFE, 1);
  EXPECT_EQ(op.route(), op.FLOOD);
  for (uint32_t deadline = 1; deadline <= 2; deadline++) {
    EXPECT_EQ(op.onTimeout(deadline), op.RETRY_FLOOD);
    ASSERT_TRUE(op.restart(op.FLOOD, deadline + 1));
  }
  EXPECT_EQ(op.onTimeout(3), op.FINISH_TIMEOUT);
}

TEST(RemoteNodeOperation, MissingKnownPathSkipsPathRetry) {
  zen::RemoteNodeOperation op;
  op.begin(op.LOGIN, nullptr, true, op.RETRY_SAFE, 1);
  EXPECT_EQ(op.onTimeout(1, false), op.RETRY_FLOOD);
}

TEST(RemoteNodeOperation, UnsafeCommandIsNeverRetried) {
  zen::RemoteNodeOperation op;
  op.begin(op.ADMIN_WRITE, nullptr, true, op.DO_NOT_RETRY, 10);
  EXPECT_EQ(op.onTimeout(10), op.FINISH_UNKNOWN);
  EXPECT_EQ(op.result(), op.RESULT_UNKNOWN);
}

TEST(RemoteNodeOperation, MatchesIdentityAndIgnoresLateCompletion) {
  zen::RemoteNodeOperation op;
  uint8_t first[32] = {1}, other[32] = {2};
  op.begin(op.ADMIN_READ, first, false, op.RETRY_SAFE, 10);
  EXPECT_TRUE(op.matches(first));
  EXPECT_FALSE(op.matches(other));
  EXPECT_TRUE(op.finish(op.SUCCESS));
  EXPECT_FALSE(op.finish(op.SUCCESS));
  EXPECT_FALSE(op.matches(first));
}

TEST(RemoteNodeOperation, DeadlineSurvivesMillisRollover) {
  zen::RemoteNodeOperation op;
  op.begin(op.LOGIN, nullptr, false, op.RETRY_SAFE, 4);
  EXPECT_EQ(op.onTimeout(UINT32_MAX), op.WAIT);
  EXPECT_EQ(op.onTimeout(3), op.WAIT);
  EXPECT_EQ(op.onTimeout(4), op.RETRY_FLOOD);
}

TEST(RemoteNodeCoordinator, EnforcesOneOnDeviceRequestAndOwner) {
  zen::RemoteNodeCoordinator coordinator;
  uint8_t first[32] = {1}, second[32] = {2};
  ASSERT_TRUE(coordinator.begin(coordinator.LOGIN_OWNER, zen::RemoteNodeOperation::LOGIN,
                                first, true, zen::RemoteNodeOperation::RETRY_SAFE, 10));
  EXPECT_FALSE(coordinator.begin(coordinator.SENSOR_OWNER,
                                 zen::RemoteNodeOperation::TELEMETRY, second, false,
                                 zen::RemoteNodeOperation::RETRY_SAFE, 10));
  EXPECT_FALSE(coordinator.complete(coordinator.LOGIN_OWNER, second,
                                    zen::RemoteNodeOperation::SUCCESS));
  EXPECT_TRUE(coordinator.complete(coordinator.LOGIN_OWNER, first,
                                   zen::RemoteNodeOperation::SUCCESS));
}

TEST(RemoteNodeCoordinator, RetainsOwnerAcrossRetryAndReleasesAtTerminalResult) {
  zen::RemoteNodeCoordinator coordinator;
  uint8_t key[32] = {3};
  ASSERT_TRUE(coordinator.begin(coordinator.ADMIN_OWNER,
                                zen::RemoteNodeOperation::ADMIN_READ, key, true,
                                zen::RemoteNodeOperation::RETRY_SAFE, 10));
  EXPECT_EQ(coordinator.timeout(coordinator.ADMIN_OWNER, 10, true),
            zen::RemoteNodeOperation::RETRY_PATH);
  ASSERT_TRUE(coordinator.restart(coordinator.ADMIN_OWNER, 20));
  EXPECT_EQ(coordinator.operation().attempt(), 2);
  coordinator.fail(coordinator.ADMIN_OWNER, zen::RemoteNodeOperation::SEND_FAILED);
  EXPECT_FALSE(coordinator.active());
  EXPECT_EQ(coordinator.operation().result(), zen::RemoteNodeOperation::SEND_FAILED);
}

TEST(RemoteNodeAdapters, ClassifyAdminSafetyAndMatchTelemetryIdentityAndTag) {
  EXPECT_EQ(zen::RemoteAdminAdapter::retryMode(true),
            zen::RemoteNodeOperation::RETRY_SAFE);
  EXPECT_EQ(zen::RemoteAdminAdapter::retryMode(false),
            zen::RemoteNodeOperation::DO_NOT_RETRY);
  uint8_t first[32] = {1}, second[32] = {2};
  EXPECT_TRUE(zen::RemoteTelemetryAdapter::matches(first, 42, first, 42));
  EXPECT_FALSE(zen::RemoteTelemetryAdapter::matches(first, 42, first, 41));
  EXPECT_FALSE(zen::RemoteTelemetryAdapter::matches(first, 42, second, 42));
}

TEST(RemoteNodeCoordinator, CancellationRejectsLateReplyAndAllowsNextOwner) {
  zen::RemoteNodeCoordinator coordinator;
  uint8_t first[32] = {1}, second[32] = {2};
  ASSERT_TRUE(coordinator.begin(coordinator.LOGIN_OWNER, zen::RemoteNodeOperation::LOGIN,
                                first, false, zen::RemoteNodeOperation::RETRY_SAFE, 10));
  EXPECT_TRUE(coordinator.cancel(coordinator.LOGIN_OWNER, first));
  EXPECT_FALSE(coordinator.complete(coordinator.LOGIN_OWNER, first,
                                    zen::RemoteNodeOperation::SUCCESS));
  EXPECT_TRUE(coordinator.begin(coordinator.SENSOR_OWNER,
                                zen::RemoteNodeOperation::TELEMETRY, second, false,
                                zen::RemoteNodeOperation::RETRY_SAFE, 20));
}

TEST(RemoteNodeAdapters, RejectsMalformedLoginResponse) {
  uint8_t response[13]{};
  response[4] = 42;
  EXPECT_FALSE(zen::RemoteLoginAdapter::valid(response, sizeof(response)));
  response[4] = 0;  // RESP_SERVER_LOGIN_OK on the wire
  EXPECT_TRUE(zen::RemoteLoginAdapter::valid(response, sizeof(response)));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
