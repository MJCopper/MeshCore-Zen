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

  Packet long_path = makeRequest(1, 5);
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
  EXPECT_FALSE(probe.isTimedOut(30099));
  EXPECT_TRUE(probe.isTimedOut(30100));
  EXPECT_FALSE(probe.isTimedOut(30101));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
