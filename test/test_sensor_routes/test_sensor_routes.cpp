#include <gtest/gtest.h>
#include "TraceRouteCandidates.h"
#include "TraceRouteHistory.h"

static mesh::Packet command(uint8_t count = 3, uint8_t width = 1, uint8_t first = 10) {
  mesh::Packet packet;
  packet._snr = 0;
  packet.header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
  packet.setPathHashSizeAndCount(width, count);
  for (uint8_t i = 0; i < packet.getPathByteLen() && i < MAX_PATH_SIZE; i++) packet.path[i] = first + i;
  packet.payload_len = 32;
  memset(packet.payload, 42, packet.payload_len);
  return packet;
}

TEST(SensorTraceRoutes, KeepsThreeDistinctCandidatesAndUpdatesDuplicatesInPlace) {
  TraceRouteCandidates routes;
  auto original = command();
  routes.begin(&original);
  routes.observe(&original);
  EXPECT_EQ(0, routes.count());
  auto alternate = command(4);
  routes.observe(&alternate);
  ASSERT_EQ(1, routes.count());
  EXPECT_EQ(4, routes.candidate(0).repeaters);
  auto shorter = command(2, 2, 20);
  routes.observe(&shorter);
  EXPECT_EQ(2, routes.candidate(1).repeaters);
  EXPECT_EQ(1, routes.candidate(1).flags);
  auto newer = command(2, 1, 30);
  routes.observe(&newer);
  EXPECT_EQ(31, routes.candidate(2).path[0]);
  routes.observe(&alternate);
  EXPECT_EQ(31, routes.candidate(2).path[0]);
  EXPECT_EQ(3, routes.count());
  routes.clear();
  routes.observe(&shorter);
  EXPECT_EQ(0, routes.count());
}

TEST(SensorTraceRoutes, RejectsDifferentPayloadChannelAndUnsupportedRoutes) {
  TraceRouteCandidates routes;
  auto original = command();
  routes.begin(&original);
  auto other = command(2);
  other.payload[0] ^= 1;  // Includes the channel hash in the command fingerprint.
  routes.observe(&other);
  EXPECT_EQ(0, routes.count());
  other = command(2);
  other.payload[20] ^= 1;
  routes.observe(&other);
  EXPECT_EQ(0, routes.count());
  for (auto invalid : { command(0), command(11), command(2, 3) }) routes.observe(&invalid);
  EXPECT_EQ(0, routes.count());
  other = command(2);
  other.header = ROUTE_TYPE_DIRECT | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
  routes.observe(&other);
  EXPECT_EQ(0, routes.count());
}

TEST(SensorTraceRoutes, StartingAnotherCommandDropsThePreviousCandidate) {
  TraceRouteCandidates routes;
  auto first = command(), alternate = command(2);
  routes.begin(&first);
  routes.observe(&alternate);
  ASSERT_EQ(1, routes.count());
  auto next = command();
  next.payload[4] = 20;
  routes.begin(&next);
  routes.observe(&alternate);
  EXPECT_EQ(0, routes.count());
}

TEST(SensorTraceRoutes, BoundedCandidatesUseRecentSuccessBeforeLength) {
  TraceRouteCandidates routes;
  TraceRouteHistory history;
  auto original = command(), short_path = command(2, 1, 20), long_path = command(4, 1, 30);
  routes.begin(&original, 100);
  routes.observe(&short_path, 200);
  routes.observe(&long_path, 300);
  uint32_t airtimes[] = { 100, 200, 0 };
  auto longer = routes.candidate(1);
  history.record(longer, 0, 123, 300, true, 5319);
  ASSERT_NE(nullptr, routes.best(history, 0, 123, 400, airtimes));
  EXPECT_TRUE(routes.best(history, 0, 123, 400, airtimes)->equals(longer));
  EXPECT_EQ(2, routes.best(history, 1, 123, 400, airtimes)->repeaters);
  EXPECT_EQ(2, routes.best(history, 0, 124, 400, airtimes)->repeaters);
  EXPECT_EQ(2, routes.best(history, 0, 123, 300 + TraceRouteHistory::TTL_MILLIS, airtimes)->repeaters);
  for (uint8_t i = 40; i < 50; i++) {
    auto next = command(2, 1, i);
    routes.observe(&next, 1000 + i);
  }
  EXPECT_EQ(3, routes.count());
}

TEST(SensorTraceRoutes, DiversityAndAirtimeBreakUnprovenRouteTies) {
  TraceRouteCandidates routes;
  TraceRouteHistory history;
  auto original = command(3, 1, 10);
  auto similar = original;
  similar.path[0] = 9;
  auto distinct = command(3, 1, 20);
  routes.begin(&original, 0);
  routes.observe(&similar, 100);
  routes.observe(&distinct, 200);
  uint32_t airtimes[] = { 100, 100, 0 };
  EXPECT_TRUE(routes.best(history, 0, 0, 300, airtimes)->equals(routes.candidate(1)));
  history.record(routes.candidate(1), 0, 0, 300, false);
  EXPECT_TRUE(routes.best(history, 0, 0, 301, airtimes)->equals(routes.candidate(0)));
  EXPECT_EQ(200u, routes.lastSeen());
  routes.observe(&original, 400);
  EXPECT_EQ(400u, routes.lastSeen());
}

static mesh::PacketScope scope(uint8_t value) {
  mesh::PacketScope result;
  result.kind = mesh::PacketScope::RESOLVED;
  result.key[0] = value;
  return result;
}

TEST(SensorTraceRoutes, HistoryUsesCompleteScopeKeyAndNeverReusesUnresolvedEvidence) {
  TraceRouteHistory history;
  auto packet = command();
  TraceRoute route{};
  ASSERT_EQ(TraceRoute::VALID, TraceRoute::fromRequest(&packet, route));
  auto a = scope(1), b = scope(1);
  b.key[15] = 2;  // same short prefix, different complete identity
  history.record(route, 0, 10, 100, true, 20, a);
  EXPECT_EQ(2, history.rank(route, 0, 10, 101, a));
  EXPECT_EQ(1, history.rank(route, 0, 10, 101, b));
  EXPECT_EQ(1, history.rank(route, 0, 10, 101));
  history.record(route, 0, 10, 102, false, 0, b);
  EXPECT_EQ(0, history.rank(route, 0, 10, 103, b));
  EXPECT_EQ(2, history.rank(route, 0, 10, 103, a));
  auto unresolved = a;
  unresolved.kind = mesh::PacketScope::UNRESOLVED;
  history.record(route, 0, 10, 104, false, 0, unresolved);
  EXPECT_EQ(1, history.rank(route, 0, 10, 105, unresolved));
  EXPECT_EQ(2, history.rank(route, 0, 10, 105, a));
  EXPECT_EQ(1, history.rank(route, 0, 10, 100 + TraceRouteHistory::TTL_MILLIS, a));
}

TEST(SensorTraceRoutes, ForeignScopeCopiesCannotExtendQuietTimeOrAddCandidates) {
  TraceRouteCandidates routes;
  auto original = command(), alternate = command(2);
  const auto a = scope(1), b = scope(2);
  routes.begin(&original, 100, a);
  routes.observe(&alternate, 200, b);
  routes.observe(&alternate, 250);  // unscoped
  EXPECT_EQ(0, routes.count());
  EXPECT_EQ(100u, routes.lastSeen());
  routes.observe(&alternate, 300, a);
  EXPECT_EQ(1, routes.count());
  EXPECT_EQ(300u, routes.lastSeen());
  routes.clear();
  routes.observe(&alternate, 400, a);
  EXPECT_EQ(0, routes.count());
  EXPECT_EQ(300u, routes.lastSeen());
}

TEST(SensorTraceRoutes, UnresolvedCandidatesRequireExactCodeTupleAndRouteClass) {
  TraceRouteCandidates routes;
  auto original = command(), alternate = command(2);
  mesh::PacketScope unknown;
  unknown.kind = mesh::PacketScope::UNRESOLVED;
  unknown.codes[0] = 123;
  unknown.route_type = ROUTE_TYPE_TRANSPORT_FLOOD;
  routes.begin(&original, 100, unknown);
  auto other = unknown;
  other.codes[1] = 456;
  routes.observe(&alternate, 200, other);
  other = unknown;
  other.route_type = ROUTE_TYPE_TRANSPORT_DIRECT;
  routes.observe(&alternate, 300, other);
  EXPECT_EQ(0, routes.count());
  EXPECT_EQ(100u, routes.lastSeen());
  routes.observe(&alternate, 400, unknown);
  EXPECT_EQ(1, routes.count());
  EXPECT_EQ(400u, routes.lastSeen());
}

TEST(SensorTraceRoutes, HistoryCannotInventAnUnobservedRoute) {
  TraceRouteCandidates routes;
  TraceRouteHistory history;
  auto original = command(), old = command(2);
  TraceRoute old_route{};
  ASSERT_EQ(TraceRoute::VALID, TraceRoute::fromRequest(&old, old_route));
  const auto a = scope(1);
  history.record(old_route, 0, 0, 100, true, 20, a);
  routes.begin(&original, 200, a);
  uint32_t airtimes[3] = {100, 100, 100};
  EXPECT_EQ(nullptr, routes.best(history, 0, 0, 201, airtimes, a));
  routes.observe(&old, 300, a);
  ASSERT_NE(nullptr, routes.best(history, 0, 0, 301, airtimes, a));
  EXPECT_TRUE(old_route.equals(*routes.best(history, 0, 0, 301, airtimes, a)));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
