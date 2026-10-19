#include <gtest/gtest.h>
#include "TraceRouteCandidates.h"

static mesh::Packet command(uint8_t count = 3, uint8_t width = 1, uint8_t first = 10) {
  mesh::Packet packet;
  packet.header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
  packet.setPathHashSizeAndCount(width, count);
  for (uint8_t i = 0; i < packet.getPathByteLen() && i < MAX_PATH_SIZE; i++) packet.path[i] = first + i;
  packet.payload_len = 32;
  memset(packet.payload, 42, packet.payload_len);
  return packet;
}

TEST(SensorTraceRoutes, KeepsOneDistinctShorterThenMoreRecentEqualLengthRoute) {
  TraceRouteCandidates routes;
  auto original = command();
  routes.begin(&original);
  routes.observe(&original);
  EXPECT_EQ(nullptr, routes.alternate());
  auto alternate = command(4);
  routes.observe(&alternate);
  ASSERT_NE(nullptr, routes.alternate());
  EXPECT_EQ(4, routes.alternate()->repeaters);
  auto shorter = command(2, 2, 20);
  routes.observe(&shorter);
  EXPECT_EQ(2, routes.alternate()->repeaters);
  EXPECT_EQ(1, routes.alternate()->flags);
  auto newer = command(2, 1, 30);
  routes.observe(&newer);
  EXPECT_EQ(31, routes.alternate()->path[0]);
  routes.observe(&alternate);
  EXPECT_EQ(31, routes.alternate()->path[0]);
  routes.clear();
  routes.observe(&shorter);
  EXPECT_EQ(nullptr, routes.alternate());
}

TEST(SensorTraceRoutes, RejectsDifferentPayloadChannelAndUnsupportedRoutes) {
  TraceRouteCandidates routes;
  auto original = command();
  routes.begin(&original);
  auto other = command(2);
  other.payload[0] ^= 1;  // Includes the channel hash in the command fingerprint.
  routes.observe(&other);
  EXPECT_EQ(nullptr, routes.alternate());
  other = command(2);
  other.payload[20] ^= 1;
  routes.observe(&other);
  EXPECT_EQ(nullptr, routes.alternate());
  for (auto invalid : { command(0), command(11), command(2, 3) }) routes.observe(&invalid);
  EXPECT_EQ(nullptr, routes.alternate());
  other = command(2);
  other.header = ROUTE_TYPE_DIRECT | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
  routes.observe(&other);
  EXPECT_EQ(nullptr, routes.alternate());
}

TEST(SensorTraceRoutes, StartingAnotherCommandDropsThePreviousCandidate) {
  TraceRouteCandidates routes;
  auto first = command(), alternate = command(2);
  routes.begin(&first);
  routes.observe(&alternate);
  ASSERT_NE(nullptr, routes.alternate());
  auto next = command();
  next.payload[4] = 20;
  routes.begin(&next);
  routes.observe(&alternate);
  EXPECT_EQ(nullptr, routes.alternate());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
