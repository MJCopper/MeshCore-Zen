#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/DiagnosticLog.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/OperationResultCoordinator.h"

TEST(DiagnosticLog, KeepsNewestSixteenEntries) {
  zen::DiagnosticLog log;
  for (uint32_t i = 0; i < 20; i++) {
    zen::OperationResult result = zen::OperationResult::make(
        zen::Operation::RADIO, zen::OperationOutcome::WARNING,
        zen::OperationReason::QUEUE_FULL);
    result.context.value = (int16_t)i;
    log.add(i, i * 100, result);
  }
  ASSERT_EQ(log.size(), 16);
  EXPECT_EQ(log.newest(0)->result.context.value, 19);
  EXPECT_EQ(log.newest(15)->result.context.value, 4);
}

TEST(DiagnosticLog, CoalescesConsecutiveDuplicates) {
  zen::DiagnosticLog log;
  zen::OperationResult result = zen::OperationResult::make(
      zen::Operation::ROOM_LOGIN, zen::OperationOutcome::TIMEOUT,
      zen::OperationReason::NO_REPLY);
  log.add(1, 100, result);
  log.add(2, 200, result);
  ASSERT_EQ(log.size(), 1);
  EXPECT_EQ(log.newest(0)->count, 2);
  EXPECT_EQ(log.newest(0)->timestamp, 2u);
}

TEST(DiagnosticLog, KeepsSeverityDistinct) {
  zen::DiagnosticLog log;
  log.add(1, 100, zen::OperationResult::make(
      zen::Operation::GPS, zen::OperationOutcome::WARNING,
      zen::OperationReason::NO_GPS_FIX));
  log.add(2, 200, zen::OperationResult::make(
      zen::Operation::GPS, zen::OperationOutcome::TRANSPORT_FAILURE,
      zen::OperationReason::NO_GPS_FIX));
  ASSERT_EQ(log.size(), 2);
  EXPECT_EQ(zen::DiagnosticLog::severity(log.newest(0)->result),
            zen::DiagnosticLog::ERROR);
  EXPECT_EQ(zen::DiagnosticLog::severity(log.newest(1)->result),
            zen::DiagnosticLog::WARNING);
}

TEST(OperationResultCoordinator, SuppressesRepeatedBackgroundPopup) {
  zen::DiagnosticLog log;
  zen::OperationResultCoordinator coordinator;
  zen::OperationResult result = zen::OperationResult::make(
      zen::Operation::GPS, zen::OperationOutcome::TRANSPORT_FAILURE,
      zen::OperationReason::APPLY_FAILED, zen::RESULT_BACKGROUND);
  auto first = coordinator.publish(log, 0, 1000, result);
  auto repeated = coordinator.publish(log, 0, 2000, result);
  EXPECT_TRUE(first.popup);
  EXPECT_FALSE(repeated.popup);
  EXPECT_TRUE(repeated.repeated);
  ASSERT_EQ(log.size(), 1);
  EXPECT_EQ(log.newest(0)->count, 2);
}

TEST(OperationResultCoordinator, ScreenStatusUsesSameCatalogText) {
  zen::OperationResult result = zen::OperationResult::make(
      zen::Operation::ADMIN, zen::OperationOutcome::INVALID_RESPONSE,
      zen::OperationReason::INVALID_SETTINGS_REPLY);
  zen::OperationStatus status;
  status.set(result);
  EXPECT_STREQ(status.text(), zen::OperationResultCatalog::status(result));
  EXPECT_STREQ(status.text(), "Invalid setting reply");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
