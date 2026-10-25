#include <gtest/gtest.h>
#include <cstring>
#include "../../examples/bme680_sensor/RepeaterTraceProbe.h"

using namespace mesh;

static Packet makeRequest(uint8_t size, uint8_t count) {
  Packet packet;
  packet.header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
  packet.setPathHashSizeAndCount(size, count);
  for (uint8_t i = 0; i < count * size && i < MAX_PATH_SIZE; i++) packet.path[i] = i + 1;
  return packet;
}

class TraceProbeTest : public ::testing::Test {
protected:
  RepeaterTraceProbe probe;
  Packet first, retry, reply;
  TraceRoute original{}, alternate{};
  RepeaterTraceProbe::Result result{};
  uint8_t snrs[19]{};
  void start(uint8_t count = 3, uint8_t size = 1, uint32_t now = 0) {
    reply._snr = 0;
    reply.header = ROUTE_TYPE_DIRECT | (PAYLOAD_TYPE_TRACE << PH_TYPE_SHIFT);
    auto request = makeRequest(size, count);
    ASSERT_EQ(RepeaterTraceProbe::STARTED, probe.start(&request, 101, 202, now));
    original = probe.route();
    for (uint8_t i = 0; i < sizeof(snrs); i++) snrs[i] = 4 * (i + 1);
  }
  void transmit(uint32_t now, bool complete = true) {
    auto packet = probe.retryStarted() ? &retry : &first;
    probe.setQueuedPacket(packet);
    ASSERT_TRUE(probe.onTxStarted(packet, now));
    if (complete) probe.onTxComplete(packet);
  }
  void beginRetry(uint32_t now = 13000, bool change_route = false) {
    ASSERT_TRUE(probe.isTimedOut(now));
    if (change_route) {
      auto request = makeRequest(2, 2);
      ASSERT_EQ(TraceRoute::VALID, TraceRoute::fromRequest(&request, alternate));
    } else alternate = original;
    ASSERT_TRUE(probe.beginRetry(303, 404, now, &alternate));
  }
  RepeaterTraceProbe::ReceiveResult receive(bool second, uint32_t now) {
    const auto& route = second ? alternate : original;
    reply.path_len = 2 * route.repeaters - 1;
    return probe.receive(&reply, second ? 303 : 101, second ? 404 : 202,
                          route.flags, snrs, route.path, route.bytes, now, result);
  }
};

TEST_F(TraceProbeTest, MirrorsRouteAndRetainsExistingResultGeometry) {
  start();
  const uint8_t expected[] = { 3, 2, 1, 2, 3 };
  EXPECT_EQ(5, probe.pathBytes());
  EXPECT_EQ(0, memcmp(expected, probe.path(), sizeof(expected)));
  transmit(1000);
  ASSERT_EQ(RepeaterTraceProbe::COMPLETE, receive(false, 3500));
  EXPECT_EQ(2500u, result.elapsed_millis);
  EXPECT_EQ(3, result.route.repeaters);
  EXPECT_EQ(0, memcmp(snrs, result.snrs, 3));
}

TEST_F(TraceProbeTest, RejectsUnsupportedRequests) {
  auto empty = makeRequest(1, 0), too_long = makeRequest(1, 11), three = makeRequest(3, 1);
  EXPECT_EQ(RepeaterTraceProbe::NO_REPEATERS, probe.start(&empty, 1, 2, 0));
  EXPECT_EQ(RepeaterTraceProbe::TOO_LONG, probe.start(&too_long, 1, 2, 0));
  EXPECT_EQ(RepeaterTraceProbe::INVALID_PATH, probe.start(&three, 1, 2, 0));
  three.header = ROUTE_TYPE_DIRECT;
  EXPECT_EQ(RepeaterTraceProbe::INVALID_PATH, probe.start(&three, 1, 2, 0));
  EXPECT_EQ(RepeaterTraceProbe::INVALID_PATH, probe.start(nullptr, 1, 2, 0));
}

TEST_F(TraceProbeTest, SupportsTenRepeatersAndBothHashWidths) {
  for (uint8_t size : { 1, 2 }) {
    start(10, size);
    EXPECT_EQ(19 * size, probe.pathBytes());
    EXPECT_EQ(size - 1, probe.flags());
    EXPECT_EQ(2 + 9 + 19 * size + 19, probe.route().maxPacketLength());
    transmit(0);
    EXPECT_EQ(RepeaterTraceProbe::COMPLETE, receive(false, 29999));
    probe.cancel();
  }
}

TEST_F(TraceProbeTest, UsesCompanionFormulaForAllSupportedRoutes) {
  const uint32_t millis[] = { 0, 3200, 4900, 6600, 8300, 10000, 11700, 13400, 15100, 16800, 18500 };
  for (uint8_t count = 1; count <= 10; count++) {
    start(count);
    probe.setAirtimeEstimate(100);
    transmit(1000);
    EXPECT_FALSE(probe.isTimedOut(1000 + millis[count] - 1));
    EXPECT_TRUE(probe.isTimedOut(1000 + millis[count]));
    EXPECT_TRUE(probe.retryPending());
    probe.cancel();
  }
}

TEST(TraceTimingPolicy, AccountsForSlowRadioAndSaturatesSafely) {
  EXPECT_EQ(6600u, TraceTimingPolicy::attemptTimeout(3, 100));
  EXPECT_EQ(39000u, TraceTimingPolicy::attemptTimeout(3, 1000));
  EXPECT_EQ(126500u, TraceTimingPolicy::attemptTimeout(10, 1000));
  EXPECT_EQ(0x7FFFF000u, TraceTimingPolicy::attemptTimeout(10, UINT32_MAX));
}

TEST_F(TraceProbeTest, QueueWaitDoesNotStartTheRoundTripClock) {
  start(2, 1, 100);
  EXPECT_FALSE(probe.isQueueTimedOut(10099));
  EXPECT_TRUE(probe.isQueueTimedOut(10100));
  EXPECT_FALSE(probe.isTimedOut(10100));
  transmit(10100);
  EXPECT_FALSE(probe.isTimedOut(12599));
  EXPECT_TRUE(probe.isTimedOut(12600));
}

TEST_F(TraceProbeTest, UntransmittedAttemptCannotProduceAResult) {
  start();
  EXPECT_EQ(RepeaterTraceProbe::IGNORED, receive(false, 100));
  probe.queueFailed(10000);
  EXPECT_TRUE(probe.failurePending());
  EXPECT_EQ(RepeaterTraceProbe::RADIO_BUSY, probe.failure());
  EXPECT_EQ(RepeaterTraceProbe::IGNORED, receive(false, 10001));
}

TEST_F(TraceProbeTest, FirstResultAfterDeadlineBeforeTimeoutTransitionIsAccepted) {
  start();
  transmit(0);
  EXPECT_EQ(RepeaterTraceProbe::COMPLETE, receive(false, 13001));
  EXPECT_EQ(13001u, result.elapsed_millis);
}

TEST_F(TraceProbeTest, FirstResultInRetryPendingStateIsAccepted) {
  start();
  transmit(0);
  ASSERT_TRUE(probe.isTimedOut(13000));
  EXPECT_EQ(RepeaterTraceProbe::COMPLETE, receive(false, 13001));
}

TEST_F(TraceProbeTest, FirstResultCanCancelAQueuedRetry) {
  start();
  transmit(0);
  beginRetry();
  probe.setQueuedPacket(&retry);
  EXPECT_EQ(RepeaterTraceProbe::COMPLETE, receive(false, 14000));
  EXPECT_EQ(&retry, probe.queuedPacket());
}

TEST_F(TraceProbeTest, FirstResultIsSavedWhileRetryIsInFlight) {
  start();
  transmit(0);
  beginRetry();
  transmit(15000);
  EXPECT_EQ(RepeaterTraceProbe::STORED, receive(false, 16000));
  RepeaterTraceProbe::Result fallback;
  ASSERT_TRUE(probe.takeFallback(fallback));
  EXPECT_EQ(16000u, fallback.elapsed_millis);
  EXPECT_FALSE(probe.takeFallback(fallback));
}

TEST_F(TraceProbeTest, SuccessfulRetryWinsAndClosingDiscardsFallback) {
  start();
  transmit(0);
  beginRetry(13000, true);
  transmit(15000);
  EXPECT_EQ(RepeaterTraceProbe::STORED, receive(false, 16000));
  ASSERT_EQ(RepeaterTraceProbe::COMPLETE, receive(true, 17000));
  EXPECT_EQ(2000u, result.elapsed_millis);
  EXPECT_TRUE(result.route.equals(alternate));
  probe.cancel();
  EXPECT_FALSE(probe.takeFallback(result));
}

TEST_F(TraceProbeTest, ChangedRouteDoesNotCorruptFirstAttemptFallback) {
  start();
  transmit(0);
  beginRetry(13000, true);
  transmit(15000);
  EXPECT_EQ(RepeaterTraceProbe::STORED, receive(false, 16000));
  ASSERT_TRUE(probe.isTimedOut(25000));
  EXPECT_TRUE(probe.isFinalizing());
  ASSERT_TRUE(probe.takeFallback(result));
  EXPECT_TRUE(result.route.equals(original));
  EXPECT_EQ(0, result.route.flags);
  EXPECT_EQ(3, result.route.repeaters);
  EXPECT_EQ(16000u, result.elapsed_millis);
}

TEST_F(TraceProbeTest, RetryTransmissionFailureRetainsFallback) {
  start();
  transmit(0);
  beginRetry();
  transmit(15000, false);
  EXPECT_EQ(RepeaterTraceProbe::STORED, receive(false, 16000));
  EXPECT_TRUE(probe.onTxFailed(&retry, 16001));
  EXPECT_TRUE(probe.takeFallback(result));
  EXPECT_EQ(16000u, result.elapsed_millis);
}

TEST_F(TraceProbeTest, RetryQueueFailureStillAcceptsTheFirstResult) {
  start();
  transmit(0);
  beginRetry();
  probe.queueFailed(23000);
  EXPECT_EQ(RepeaterTraceProbe::RETRY_NOT_SENT, probe.failure());
  EXPECT_EQ(RepeaterTraceProbe::COMPLETE, receive(false, 23001));
  EXPECT_EQ(RepeaterTraceProbe::IGNORED, receive(true, 23001));
}

TEST_F(TraceProbeTest, RetryResultAfterDeadlineBeforeFinalTransitionIsAccepted) {
  start();
  transmit(0);
  beginRetry();
  transmit(15000);
  EXPECT_EQ(RepeaterTraceProbe::COMPLETE, receive(true, 28001));
  EXPECT_EQ(13001u, result.elapsed_millis);
}

TEST_F(TraceProbeTest, EitherAttemptCanSucceedDuringFinalGrace) {
  for (bool second : { false, true }) {
    start();
    transmit(0);
    beginRetry();
    transmit(15000);
    ASSERT_TRUE(probe.isTimedOut(28000));
    EXPECT_EQ(RepeaterTraceProbe::COMPLETE, receive(second, 28001));
    probe.cancel();
  }
}

TEST_F(TraceProbeTest, EitherAttemptCanReplaceAnUncommittedFailure) {
  for (bool second : { false, true }) {
    start();
    transmit(0);
    beginRetry();
    transmit(15000);
    ASSERT_TRUE(probe.isTimedOut(28000));
    probe.advanceFinal(30999);
    EXPECT_FALSE(probe.failurePending());
    probe.advanceFinal(31000);
    EXPECT_TRUE(probe.failurePending());
    EXPECT_EQ(RepeaterTraceProbe::COMPLETE, receive(second, 31001));
    probe.cancel();
    EXPECT_EQ(RepeaterTraceProbe::IGNORED, receive(second, 31002));
  }
}

TEST_F(TraceProbeTest, FailureReportHasABoundedDeadlineThatDoesNotMove) {
  start();
  probe.queueFailed(10000);
  probe.advanceFinal(15000);
  EXPECT_FALSE(probe.reportExpired(19999));
  EXPECT_TRUE(probe.reportExpired(20000));
}

TEST_F(TraceProbeTest, RejectsWrongIdentityPathFlagsAndSnrCount) {
  start();
  transmit(0);
  reply.path_len = 5;
  EXPECT_EQ(RepeaterTraceProbe::IGNORED, probe.receive(&reply, 101, 999, 0,
      snrs, original.path, original.bytes, 100, result));
  EXPECT_EQ(RepeaterTraceProbe::IGNORED, probe.receive(&reply, 999, 202, 0,
      snrs, original.path, original.bytes, 100, result));
  EXPECT_EQ(RepeaterTraceProbe::IGNORED, probe.receive(&reply, 101, 202, 1,
      snrs, original.path, original.bytes, 100, result));
  EXPECT_EQ(RepeaterTraceProbe::IGNORED, probe.receive(&reply, 101, 202, 0,
      snrs, original.path, original.bytes - 1, 100, result));
  auto wrong = original;
  wrong.path[0] ^= 1;
  EXPECT_EQ(RepeaterTraceProbe::IGNORED, probe.receive(&reply, 101, 202, 0,
      snrs, wrong.path, wrong.bytes, 100, result));
  reply.path_len = 4;
  EXPECT_EQ(RepeaterTraceProbe::IGNORED, probe.receive(&reply, 101, 202, 0, snrs,
      original.path, original.bytes, 100, result));
}

TEST_F(TraceProbeTest, RejectsNullDataAndUnrelatedTransmissionCallbacks) {
  start();
  Packet unrelated;
  probe.setQueuedPacket(&first);
  EXPECT_FALSE(probe.onTxStarted(&unrelated, 0));
  EXPECT_FALSE(probe.onTxFailed(&unrelated, 0));
  transmit(0);
  reply.path_len = 5;
  EXPECT_EQ(RepeaterTraceProbe::IGNORED, probe.receive(nullptr, 101, 202, 0,
      snrs, original.path, original.bytes, 100, result));
  EXPECT_EQ(RepeaterTraceProbe::IGNORED, probe.receive(&reply, 101, 202, 0,
      nullptr, original.path, original.bytes, 100, result));
  EXPECT_EQ(RepeaterTraceProbe::IGNORED, probe.receive(&reply, 101, 202, 0,
      snrs, nullptr, original.bytes, 100, result));
  reply.header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_TRACE << PH_TYPE_SHIFT);
  EXPECT_EQ(RepeaterTraceProbe::IGNORED, receive(false, 100));
  reply.header = ROUTE_TYPE_DIRECT | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
  EXPECT_EQ(RepeaterTraceProbe::IGNORED, receive(false, 100));
}

TEST_F(TraceProbeTest, TimersWorkAcrossMillisRollover) {
  uint32_t now = UINT32_MAX - 5000;
  start(3, 1, now);
  transmit(now);
  EXPECT_FALSE(probe.isTimedOut(now + 2999));
  EXPECT_TRUE(probe.isTimedOut(now + 3000));
  EXPECT_EQ(RepeaterTraceProbe::COMPLETE, receive(false, now + 3001));
  EXPECT_EQ(3001u, result.elapsed_millis);
}

TEST_F(TraceProbeTest, RetryIdentifiersAreDistinctEvenWithRepeatedRandomInput) {
  start();
  transmit(0);
  ASSERT_TRUE(probe.isTimedOut(13000));
  ASSERT_TRUE(probe.beginRetry(101, 202, 13000));
  EXPECT_NE(101u, probe.tag());
}

TEST_F(TraceProbeTest, CannotStartAThirdProbeOrANewTransactionBeforeClosing) {
  start();
  transmit(0);
  beginRetry();
  transmit(15000);
  EXPECT_TRUE(probe.isTimedOut(28000));
  EXPECT_FALSE(probe.beginRetry(505, 606, 28000));
  auto request = makeRequest(1, 3);
  EXPECT_EQ(RepeaterTraceProbe::BUSY, probe.start(&request, 505, 606, 28000));
  probe.cancel();
  EXPECT_EQ(RepeaterTraceProbe::STARTED, probe.start(&request, 505, 606, 28000));
  EXPECT_EQ(RepeaterTraceProbe::IGNORED, receive(false, 28001));
}

TEST_F(TraceProbeTest, InvalidAlternateCannotReachTheRadio) {
  start();
  transmit(0);
  ASSERT_TRUE(probe.isTimedOut(13000));
  TraceRoute invalid{};
  EXPECT_FALSE(probe.beginRetry(303, 404, 13000, &invalid));
  invalid = original;
  invalid.flags = 2;
  EXPECT_FALSE(probe.beginRetry(303, 404, 13000, &invalid));
  invalid = original;
  invalid.bytes--;
  EXPECT_FALSE(probe.beginRetry(303, 404, 13000, &invalid));
}

TEST_F(TraceProbeTest, FinalGraceAndReportDeadlineWorkAcrossRollover) {
  uint32_t now = UINT32_MAX - 1500;
  start(3, 1, now);
  transmit(now, false);
  EXPECT_TRUE(probe.onTxFailed(&first, now));
  probe.advanceFinal(now + 2999);
  EXPECT_FALSE(probe.failurePending());
  probe.advanceFinal(now + 3000);
  EXPECT_TRUE(probe.failurePending());
  EXPECT_FALSE(probe.reportExpired(now + 12999));
  EXPECT_TRUE(probe.reportExpired(now + 13000));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
