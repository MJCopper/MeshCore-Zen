#include <gtest/gtest.h>
#include <cstring>
#include "../../examples/simple_sensor/RepeaterTraceProbe.h"

using namespace mesh;

static Packet makeRequest(uint8_t hash_size, uint8_t count) {
  Packet packet;
  packet.header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
  packet.setPathHashSizeAndCount(hash_size, count);
  for (uint8_t i = 0; i < count * hash_size; i++) packet.path[i] = i + 1;
  return packet;
}

TEST(RepeaterTraceProbe, MirrorsIncomingRepeaterOrderAndMeasuresRoundTrip) {
  RepeaterTraceProbe probe;
  Packet request = makeRequest(1, 3);
  EXPECT_EQ(RepeaterTraceProbe::STARTED, probe.start(&request, 123, 456, 1000));
  EXPECT_EQ(0, probe.flags());
  EXPECT_EQ(5, probe.pathBytes());
  const uint8_t expected[] = { 3, 2, 1, 2, 3 };
  EXPECT_EQ(0, memcmp(expected, probe.path(), sizeof(expected)));

  Packet reply;
  Packet outbound;
  probe.setQueuedPacket(&outbound);
  EXPECT_TRUE(probe.onTxStarted(&outbound, 1000));
  reply.path_len = 5;
  uint32_t elapsed = 0;
  uint8_t repeaters = 0;
  EXPECT_FALSE(probe.complete(&reply, 123, 457, 0, expected, sizeof(expected),
                              3500, elapsed, repeaters));
  EXPECT_TRUE(probe.complete(&reply, 123, 456, 0, expected, sizeof(expected),
                             3500, elapsed, repeaters));
  EXPECT_EQ(2500u, elapsed);
  EXPECT_EQ(3, repeaters);
}

TEST(RepeaterTraceProbe, RejectsUnmeasurableAndUnsupportedPaths) {
  RepeaterTraceProbe probe;
  Packet direct = makeRequest(1, 0);
  EXPECT_EQ(RepeaterTraceProbe::NO_REPEATERS, probe.start(&direct, 1, 2, 0));

  Packet long_path = makeRequest(1, 11);
  EXPECT_EQ(RepeaterTraceProbe::TOO_LONG, probe.start(&long_path, 1, 2, 0));

  Packet three_byte_hashes = makeRequest(3, 1);
  EXPECT_EQ(RepeaterTraceProbe::INVALID_PATH,
            probe.start(&three_byte_hashes, 1, 2, 0));
}

TEST(RepeaterTraceProbe, SupportsTwoByteHashesAndTimesOut) {
  RepeaterTraceProbe probe;
  Packet request = makeRequest(2, 2);
  EXPECT_EQ(RepeaterTraceProbe::STARTED, probe.start(&request, 3, 4, 100));
  EXPECT_EQ(1, probe.flags());
  const uint8_t expected[] = { 3, 4, 1, 2, 3, 4 };
  EXPECT_EQ(sizeof(expected), probe.pathBytes());
  EXPECT_EQ(0, memcmp(expected, probe.path(), sizeof(expected)));
  Packet outbound;
  probe.setQueuedPacket(&outbound);
  EXPECT_TRUE(probe.onTxStarted(&outbound, 100));
  EXPECT_FALSE(probe.isTimedOut(20099));
  EXPECT_TRUE(probe.isTimedOut(20100));
  EXPECT_FALSE(probe.isTimedOut(20101));
}

TEST(RepeaterTraceProbe, SupportsTenRepeatersWithTwoByteHashes) {
  RepeaterTraceProbe probe;
  Packet request = makeRequest(2, 10);
  EXPECT_EQ(RepeaterTraceProbe::STARTED, probe.start(&request, 7, 8, 1000));
  EXPECT_EQ(38, probe.pathBytes());
  EXPECT_EQ(1, probe.flags());
  const uint8_t* path = probe.path();
  for (uint8_t i = 0; i < 19; i++) {
    uint8_t source = i < 10 ? 9 - i : i - 9;
    EXPECT_EQ(request.path[source * 2], path[i * 2]);
    EXPECT_EQ(request.path[source * 2 + 1], path[i * 2 + 1]);
  }

  Packet reply;
  Packet outbound;
  probe.setQueuedPacket(&outbound);
  EXPECT_TRUE(probe.onTxStarted(&outbound, 1000));
  reply.path_len = 19;
  uint32_t elapsed = 0;
  uint8_t repeaters = 0;
  EXPECT_FALSE(probe.isTimedOut(30999));
  EXPECT_TRUE(probe.complete(&reply, 7, 8, 1, path, 38, 30999,
                             elapsed, repeaters));
  EXPECT_EQ(29999u, elapsed);
  EXPECT_EQ(10, repeaters);
}

TEST(RepeaterTraceProbe, SupportsTenRepeatersWithOneByteHashes) {
  RepeaterTraceProbe probe;
  Packet request = makeRequest(1, 10);
  EXPECT_EQ(RepeaterTraceProbe::STARTED, probe.start(&request, 20, 21, 0));
  EXPECT_EQ(19, probe.pathBytes());
  EXPECT_EQ(0, probe.flags());
  for (uint8_t i = 0; i < 19; i++) {
    uint8_t source = i < 10 ? 9 - i : i - 9;
    EXPECT_EQ(request.path[source], probe.path()[i]);
  }
}

TEST(RepeaterTraceProbe, LongRouteTimesOutAndAllowsManualRetry) {
  RepeaterTraceProbe probe;
  Packet request = makeRequest(1, 5);
  EXPECT_EQ(RepeaterTraceProbe::STARTED, probe.start(&request, 10, 11, 0));
  Packet outbound;
  probe.setQueuedPacket(&outbound);
  EXPECT_TRUE(probe.onTxStarted(&outbound, 0));
  EXPECT_FALSE(probe.isTimedOut(29999));

  Packet reply;
  reply.path_len = 9;
  uint32_t elapsed = 0;
  uint8_t repeaters = 0;
  EXPECT_FALSE(probe.complete(&reply, 10, 11, 0, probe.path(), probe.pathBytes(),
                              30000, elapsed, repeaters));
  EXPECT_TRUE(probe.isTimedOut(30000));
  EXPECT_FALSE(probe.isTimedOut(30001));
  EXPECT_EQ(RepeaterTraceProbe::STARTED, probe.start(&request, 12, 13, 30001));
}

TEST(RepeaterTraceProbe, FourRepeatersUseShortTimeout) {
  RepeaterTraceProbe probe;
  Packet request = makeRequest(1, 4);
  EXPECT_EQ(RepeaterTraceProbe::STARTED, probe.start(&request, 1, 2, 500));
  Packet outbound;
  probe.setQueuedPacket(&outbound);
  EXPECT_TRUE(probe.onTxStarted(&outbound, 500));
  EXPECT_FALSE(probe.isTimedOut(20499));
  EXPECT_TRUE(probe.isTimedOut(20500));
}

TEST(RepeaterTraceProbe, QueueDeadlineDoesNotStartRoundTripClock) {
  RepeaterTraceProbe probe;
  Packet request = makeRequest(1, 2);
  Packet outbound;
  EXPECT_EQ(RepeaterTraceProbe::STARTED, probe.start(&request, 1, 2, 100));
  probe.setQueuedPacket(&outbound);
  EXPECT_FALSE(probe.isTimedOut(10099));
  EXPECT_FALSE(probe.isQueueTimedOut(10099));
  EXPECT_TRUE(probe.isQueueTimedOut(10100));
  EXPECT_TRUE(probe.onTxStarted(&outbound, 10100));
  EXPECT_FALSE(probe.isQueueTimedOut(10101));
  EXPECT_FALSE(probe.isTimedOut(30099));
  EXPECT_TRUE(probe.isTimedOut(30100));
}

TEST(RepeaterTraceProbe, FailedTransmissionAllowsManualRetry) {
  RepeaterTraceProbe probe;
  Packet request = makeRequest(1, 2);
  Packet outbound;
  Packet unrelated;
  EXPECT_EQ(RepeaterTraceProbe::STARTED, probe.start(&request, 1, 2, 0));
  probe.setQueuedPacket(&outbound);
  EXPECT_FALSE(probe.onTxStarted(&unrelated, 50));
  EXPECT_FALSE(probe.onTxFailed(&unrelated));
  EXPECT_TRUE(probe.onTxStarted(&outbound, 100));
  EXPECT_TRUE(probe.onTxFailed(&outbound));
  EXPECT_FALSE(probe.isActive());
  EXPECT_EQ(RepeaterTraceProbe::STARTED, probe.start(&request, 3, 4, 101));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
