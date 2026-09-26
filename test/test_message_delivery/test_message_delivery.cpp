#include <gtest/gtest.h>

#define MAX_TEXT_LEN 160
#define OUT_PATH_UNKNOWN 255
#include "../../examples/companion_radio/zen-overlay/app/zen/MessageDeliveryCoordinator.h"

TEST(MessageDeliveryCoordinator, ClassifiesRoutesAndAddsAckMargin) {
  EXPECT_EQ(zen::ROUTE_FLOOD,
            zen::MessageDeliveryCoordinator::routeForPath(OUT_PATH_UNKNOWN));
  EXPECT_EQ(zen::ROUTE_DIRECT,
            zen::MessageDeliveryCoordinator::routeForPath(0));
  EXPECT_EQ(zen::ROUTE_PATH,
            zen::MessageDeliveryCoordinator::routeForPath(2));
  EXPECT_EQ(9000u, zen::MessageDeliveryCoordinator::deadline(3000, 2000));
}

TEST(MessageDeliveryCoordinator, AcceptsAckFromAnyRecordedAttempt) {
  zen::DirectMessageRecord entry{};
  entry.outgoing = 1;
  zen::MessageDeliveryCoordinator::recordAttempt(
      entry, 0, 101, 5000, zen::ROUTE_PATH);
  zen::MessageDeliveryCoordinator::recordAttempt(
      entry, 1, 202, 6000, zen::ROUTE_FLOOD);

  uint8_t route = zen::ROUTE_NONE;
  EXPECT_TRUE(zen::MessageDeliveryCoordinator::acknowledge(entry, 101, route));
  EXPECT_EQ(zen::ROUTE_PATH, route);
  EXPECT_EQ(zen::DELIVERY_OK, entry.ack_status);
}

TEST(MessageDeliveryCoordinator, BeginsKnownAndFloodDeliveryPolicies) {
  zen::DirectMessageRecord known{};
  zen::MessageDeliveryCoordinator::begin(
      known, true, 101, 5000, zen::ROUTE_PATH);
  EXPECT_EQ(zen::DELIVERY_PENDING, known.ack_status);
  EXPECT_EQ(zen::NodeRouteRetry::RETRY_PATH,
            zen::MessageDeliveryCoordinator::nextRetry(known, true));

  zen::DirectMessageRecord flood{};
  zen::MessageDeliveryCoordinator::begin(
      flood, true, 202, 5000, zen::ROUTE_FLOOD);
  EXPECT_EQ(zen::NodeRouteRetry::RETRY_FLOOD,
            zen::MessageDeliveryCoordinator::nextRetry(flood, false));

  zen::DirectMessageRecord incoming{};
  zen::MessageDeliveryCoordinator::begin(
      incoming, false, 303, 5000, zen::ROUTE_PATH);
  EXPECT_EQ(zen::DELIVERY_NONE, incoming.ack_status);
  EXPECT_TRUE(incoming.route_retry.exhausted());
}

TEST(MessageDeliveryCoordinator, TracksChannelRelayOutcome) {
  zen::ChannelMessageRecord entry{};
  zen::MessageDeliveryCoordinator::armRelay(entry, 42);
  EXPECT_EQ(zen::DELIVERY_PENDING, entry.relay_status);
  EXPECT_TRUE(zen::MessageDeliveryCoordinator::hearRelay(entry, 42));
  EXPECT_EQ(zen::DELIVERY_OK, entry.relay_status);
  EXPECT_EQ(1, entry.relay_count);

  zen::MessageDeliveryCoordinator::armRelay(entry, 43);
  EXPECT_TRUE(zen::MessageDeliveryCoordinator::expireRelay(entry, 43));
  EXPECT_EQ(zen::DELIVERY_FAIL, entry.relay_status);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
