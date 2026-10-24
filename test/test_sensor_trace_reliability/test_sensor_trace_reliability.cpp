#include <gtest/gtest.h>
#include "TraceReportDelivery.h"
#include "TraceRouteHistory.h"
#include "TraceDiagnostics.h"
#include <string>

static TraceRoute route(uint8_t first = 1, uint8_t count = 3, uint8_t width = 1) {
  mesh::Packet request;
  request.header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
  request.setPathHashSizeAndCount(width, count);
  for (uint8_t i = 0; i < count * width; i++) request.path[i] = first + i;
  TraceRoute result{};
  EXPECT_EQ(TraceRoute::VALID, TraceRoute::fromRequest(&request, result));
  return result;
}

TEST(SensorTraceDelivery, RetainsFrozenTextUntilActualCompletion) {
  TraceReportDelivery report;
  char name[] = "Sensor";
  ASSERT_TRUE(report.begin("Trace: 100 ms RTT", name, SensorReplyContext{1}, 0, 1000, 30000));
  name[0] = 'X';
  EXPECT_STREQ("Sensor: Trace: 100 ms RTT", report.text());
  EXPECT_FALSE(report.ready(999));
  EXPECT_EQ(1, report.channel());
  int packet = 0, wrong = 0;
  report.queued(&packet);
  EXPECT_FALSE(report.started(&wrong));
  EXPECT_FALSE(report.completed(&packet, 1000));
  EXPECT_TRUE(report.started(&packet));
  EXPECT_TRUE(report.completed(&packet, 1100));
  EXPECT_FALSE(report.active());
  EXPECT_EQ(TraceReportDelivery::DELIVERED, report.outcome());
  EXPECT_FALSE(report.failed(&packet, 1200));
}

TEST(SensorTraceDelivery, AllocationRetriesDoNotLoseTextOrMoveTheDeadline) {
  TraceReportDelivery report;
  ASSERT_TRUE(report.begin("Result", "Sensor", SensorReplyContext{0}, 100, 1500, 30000));
  report.retry(100);
  EXPECT_FALSE(report.ready(1599));
  EXPECT_TRUE(report.ready(1600));
  report.retry(1600);
  EXPECT_FALSE(report.ready(2099));
  EXPECT_TRUE(report.ready(2100));
  EXPECT_STREQ("Sensor: Result", report.text());
  EXPECT_TRUE(report.expired(10100));
  report.expire();
  EXPECT_EQ(TraceReportDelivery::EXPIRED, report.outcome());
}

TEST(SensorTraceDelivery, RetriesRejectedStartsButNeverRepeatsAnUncertainTransmission) {
  TraceReportDelivery report;
  int packet = 0;
  ASSERT_TRUE(report.begin("Result", "Sensor", SensorReplyContext{0}, 0, 0, 30000));
  report.queued(&packet);
  EXPECT_TRUE(report.failed(&packet, 100, true));
  EXPECT_FALSE(report.ready(349));
  EXPECT_TRUE(report.ready(350));
  report.queued(&packet);
  EXPECT_TRUE(report.started(&packet));
  EXPECT_TRUE(report.failed(&packet, 400));
  EXPECT_FALSE(report.active());
  EXPECT_EQ(TraceReportDelivery::UNCERTAIN, report.outcome());
}

TEST(SensorTraceDelivery, MultipartGapStartsAtCompletionAndHasNoUnsignedUnderflow) {
  TraceReportDelivery report;
  std::string text;
  for (int i = 0; i < 12; i++) text += "1 Ridge +1.0 dB\n";
  int first = 0, second = 0;
  ASSERT_TRUE(report.begin(text.c_str(), "Sensor", SensorReplyContext{0}, 0, 0, 30000));
  EXPECT_NE(std::string::npos, std::string(report.text()).find("1/2"));
  report.queued(&first);
  report.started(&first);
  report.completed(&first, 5000);
  EXPECT_TRUE(report.active());
  EXPECT_FALSE(report.expired(5000));
  EXPECT_FALSE(report.expired(7999));
  EXPECT_FALSE(report.ready(7999));
  EXPECT_TRUE(report.ready(8000));
  EXPECT_NE(std::string::npos, std::string(report.text()).find("2/2"));
  EXPECT_FALSE(report.completed(&first, 8000));
  report.queued(&second);
  report.started(&second);
  report.completed(&second, 9000);
  EXPECT_EQ(TraceReportDelivery::DELIVERED, report.outcome());
}

TEST(SensorTraceDelivery, ExpiryWaitsForAnActiveTransmissionAndWorksAcrossWrap) {
  TraceReportDelivery report;
  int packet = 0;
  uint32_t now = UINT32_MAX - 1000;
  ASSERT_TRUE(report.begin("Result", "Sensor", SensorReplyContext{0}, now, 1500, 30000));
  EXPECT_FALSE(report.ready(now + 1499));
  EXPECT_TRUE(report.ready(now + 1500));
  report.queued(&packet);
  report.started(&packet);
  EXPECT_FALSE(report.expired(now + 40000));
  report.completed(&packet, now + 40001);
  EXPECT_EQ(TraceReportDelivery::DELIVERED, report.outcome());
  ASSERT_TRUE(report.begin("Result", "Sensor", SensorReplyContext{0}, now, 0, 30000));
  EXPECT_FALSE(report.expired(now + 9999));
  EXPECT_TRUE(report.expired(now + 10000));
}

TEST(SensorTraceDelivery, UnknownRejectedStartIsConservativelyUncertain) {
  TraceReportDelivery report;
  int packet = 0;
  ASSERT_TRUE(report.begin("Result", "Sensor", SensorReplyContext{0}, 0, 0, 30000));
  report.queued(&packet);
  EXPECT_TRUE(report.failed(&packet, 100));
  EXPECT_FALSE(report.active());
  EXPECT_EQ(TraceReportDelivery::UNCERTAIN, report.outcome());
}

TEST(SensorTraceDelivery, SecondPartCanRecoverWithoutRepeatingTheFirstPart) {
  TraceReportDelivery report;
  std::string text(170, 'x');
  int first = 0, second = 0;
  ASSERT_TRUE(report.begin(text.c_str(), "Sensor", SensorReplyContext{1}, 0, 0, 30000));
  report.queued(&first); report.started(&first); report.completed(&first, 1000);
  EXPECT_TRUE(report.ready(4000));
  report.queued(&second); report.failed(&second, 4000, true);
  EXPECT_FALSE(report.ready(4249));
  EXPECT_TRUE(report.ready(4250));
  EXPECT_NE(std::string::npos, std::string(report.text()).find("2/2"));
  report.queued(&second); report.started(&second); report.completed(&second, 4500);
  EXPECT_EQ(TraceReportDelivery::DELIVERED, report.outcome());
}

TEST(SensorTraceDelivery, OverallDeadlineCannotBeExtendedByMultipartProgress) {
  TraceReportDelivery report;
  std::string text(170, 'x');
  int packet = 0;
  ASSERT_TRUE(report.begin(text.c_str(), "Sensor", SensorReplyContext{0}, 0, 0, 12000));
  report.queued(&packet); report.started(&packet); report.completed(&packet, 9000);
  EXPECT_FALSE(report.expired(11999));
  EXPECT_TRUE(report.expired(12000));
}

TEST(SensorTraceTiming, BoundedAirtimeSettlingBackoffAndReportAllowance) {
  EXPECT_EQ(300u, TraceTimingPolicy::settleDelay(1, 0));
  EXPECT_EQ(2250u, TraceTimingPolicy::settleDelay(UINT32_MAX, 250));
  EXPECT_EQ(150u, TraceTimingPolicy::quietInterval(0));
  EXPECT_EQ(750u, TraceTimingPolicy::quietInterval(UINT32_MAX));
  EXPECT_EQ(250u, TraceTimingPolicy::localBackoff(0));
  EXPECT_EQ(500u, TraceTimingPolicy::localBackoff(1));
  EXPECT_EQ(1000u, TraceTimingPolicy::localBackoff(255));
  EXPECT_EQ(30000u, TraceTimingPolicy::reportBudget(100));
  EXPECT_EQ(41000u, TraceTimingPolicy::reportBudget(5000));
  EXPECT_EQ(0x7FFFF000u, TraceTimingPolicy::reportBudget(UINT32_MAX));
}

TEST(SensorTraceHistory, ExactContextExpiryLateSuccessAndCapacityAreBounded) {
  TraceRouteHistory history;
  auto first = route();
  EXPECT_EQ(1, history.rank(first, 0, 123, 0));
  history.record(first, 0, 123, 10, false);
  EXPECT_EQ(0, history.rank(first, 0, 123, 10));
  history.record(first, 0, 123, 20, true, 5319);
  EXPECT_EQ(2, history.rank(first, 0, 123, 21));
  EXPECT_EQ(1, history.rank(first, 1, 123, 21));
  EXPECT_EQ(1, history.rank(first, 0, 124, 21));
  EXPECT_EQ(1, history.rank(first, 0, 123, 20 + TraceRouteHistory::TTL_MILLIS));
  for (uint8_t i = 10; i < 15; i++) history.record(route(i), 0, 123, i, true);
  EXPECT_EQ(1, history.rank(route(10), 0, 123, 21));
  EXPECT_EQ(2, history.rank(route(14), 0, 123, 21));
  history.clear();
  EXPECT_EQ(1, history.rank(route(14), 0, 123, 21));
}

TEST(SensorTraceHistory, RecentEvidenceWorksAcrossMillisWrap) {
  TraceRouteHistory history;
  auto value = route();
  uint32_t now = UINT32_MAX - 100;
  history.record(value, 0, 0, now, true);
  EXPECT_EQ(2, history.rank(value, 0, 0, now + 200));
  EXPECT_EQ(1, history.rank(value, 0, 0, now + TraceRouteHistory::TTL_MILLIS));
}

TEST(SensorTraceDiagnostics, AllDirectionsAndProgressGeometryForEverySupportedSize) {
  for (uint8_t width : { 1, 2 }) for (uint8_t hops = 1; hops <= 10; hops++) {
    mesh::Packet request;
    request.header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
    request.setPathHashSizeAndCount(width, hops);
    memset(request.path, 1, hops * width);
    RepeaterTraceProbe probe;
    ASSERT_EQ(RepeaterTraceProbe::STARTED, probe.start(&request, 11, 22, 0));
    mesh::Packet tx, reply;
    probe.setQueuedPacket(&tx);
    ASSERT_TRUE(probe.onTxStarted(&tx, 100));
    probe.onTxComplete(&tx);
    reply.header = ROUTE_TYPE_DIRECT | (PAYLOAD_TYPE_TRACE << PH_TYPE_SHIFT);
    reply.payload_len = 9 + probe.pathBytes();
    uint32_t tag = 11, auth = 22;
    memcpy(reply.payload, &tag, 4); memcpy(reply.payload + 4, &auth, 4);
    reply.payload[8] = probe.flags();
    memcpy(reply.payload + 9, probe.path(), probe.pathBytes());
    reply.path_len = 2 * hops - 1;
    reply._snr = -12;
    for (uint8_t i = 0; i < reply.path_len; i++) reply.path[i] = (uint8_t)(int8_t)(-40 + i);
    TraceDiagnostics diagnostics;
    diagnostics.observe(&reply, probe, 200);
    EXPECT_EQ(reply.path_len, diagnostics.progress[0].furthest);
    EXPECT_EQ(100u, diagnostics.progress[0].elapsed);
    EXPECT_FALSE(diagnostics.progress[0].complete);
    RepeaterTraceProbe::Result result;
    ASSERT_EQ(RepeaterTraceProbe::COMPLETE, probe.receive(&reply, 11, 22, probe.flags(),
        reply.path, probe.path(), probe.pathBytes(), 200, result));
    EXPECT_EQ(0, memcmp(reply.path, result.snrs, reply.path_len));
    EXPECT_EQ((uint8_t)(int8_t)-12, result.snrs[reply.path_len]);
    reply.path_len = 63;
    diagnostics.observe(&reply, probe, 300);
    EXPECT_EQ(2 * hops - 1, diagnostics.progress[0].furthest);
    reply.path_len = 0;
    diagnostics.observe(&reply, probe, 300);
    reply.path_len = 1; reply.payload[0] ^= 1;
    diagnostics.observe(&reply, probe, 300);
    EXPECT_EQ(100u, diagnostics.progress[0].elapsed);
    probe.cancel();
    reply.payload[0] ^= 1;
    diagnostics.observe(&reply, probe, 400);
    EXPECT_EQ(100u, diagnostics.progress[0].elapsed);
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
