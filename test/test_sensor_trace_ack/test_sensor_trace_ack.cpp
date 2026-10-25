#include "../sensor_mocks/SensorIntegrationFixture.h"
#include "TraceTransactionBudget.h"
#include "TraceRequestAcknowledgement.h"
#include <tuple>

TEST(TraceBudget, FixedLimitAndReportReserveAcrossWrap) {
  EXPECT_EQ(60u, TraceTransactionBudget::seconds());
  uint32_t start = UINT32_MAX - 500;
  uint32_t end = start + TraceTransactionBudget::DURATION_MILLIS;
  EXPECT_EQ(60000u, TraceTransactionBudget::remaining(start, end));
  EXPECT_EQ(55000u, TraceTransactionBudget::workRemaining(start, end));
  EXPECT_EQ(1u, TraceTransactionBudget::workRemaining(end - 5001, end));
  EXPECT_EQ(0u, TraceTransactionBudget::workRemaining(end - 5000, end));
  EXPECT_EQ(5000u, TraceTransactionBudget::remaining(end - 5000, end));
  EXPECT_EQ(1u, TraceTransactionBudget::remaining(end - 1, end));
  EXPECT_EQ(0u, TraceTransactionBudget::remaining(end, end));
  EXPECT_EQ(0u, TraceTransactionBudget::remaining(end + 1, end));
}

TEST(TraceBudget, AttemptsShareLiveWorkRatherThanOpeningFreshWindows) {
  EXPECT_EQ(13750u, TraceTransactionBudget::attemptWindow(100, 60100, 4));
  EXPECT_EQ(13666u, TraceTransactionBudget::attemptWindow(14100, 60100, 3));
  EXPECT_EQ(17500u, TraceTransactionBudget::attemptWindow(20100, 60100, 2));
  EXPECT_EQ(15000u, TraceTransactionBudget::attemptWindow(40100, 60100, 1));
  EXPECT_EQ(0u, TraceTransactionBudget::attemptWindow(55100, 60100, 1));
  EXPECT_EQ(0u, TraceTransactionBudget::attemptWindow(100, 60100, 0));
}

class RouteBudgets : public ::testing::TestWithParam<std::tuple<int,int>> {};
TEST_P(RouteBudgets, EverySupportedRouteStillUsesTheSameSixtySecondCeiling) {
  int width = std::get<0>(GetParam()), hops = std::get<1>(GetParam());
  uint8_t key[32]{}, via[18]{};
  TraceRoute route{};
  ASSERT_EQ(TraceRoute::VALID, TraceRoute::fromOutbound(via, ((width - 1) << 6) | (hops - 1), key, route));
  EXPECT_EQ(60u, TraceTransactionBudget::seconds());
  EXPECT_LT(TraceTransactionBudget::attemptWindow(100, 60100, 2), 55000u);
}
INSTANTIATE_TEST_SUITE_P(AllLengthsAndWidths, RouteBudgets,
    ::testing::Combine(::testing::Values(1,2), ::testing::Range(1,11)));

TEST(TraceAckState, TextOwnershipRetriesAndNoRepeatAfterPossibleEmission) {
  TraceRequestAcknowledgement ack;
  int a, other;
  ack.begin(100, 617, 3268);
  EXPECT_STREQ("Trace requested, please wait up to 617 seconds.", ack.text());
  EXPECT_TRUE(ack.ready(100));
  EXPECT_FALSE(ack.queued(&a, 3269));
  EXPECT_TRUE(ack.queued(&a, 3268)); EXPECT_FALSE(ack.started(&other, 200));
  ack.failed(&a, 200, true);
  EXPECT_FALSE(ack.ready(449)); EXPECT_TRUE(ack.ready(450));
  ack.queued(&a); ack.failed(&a, 500, false);
  EXPECT_FALSE(ack.active());
  ack.begin(600, 164, 3268); ack.queued(&a);
  EXPECT_TRUE(ack.started(&a, 700));
  ack.failed(&a, 800, true); EXPECT_FALSE(ack.active());
  ack.completed(&a); EXPECT_FALSE(ack.active());
}

TEST(TraceAckState, AdmissionAndWatchdogAreBoundedAcrossWrap) {
  TraceRequestAcknowledgement ack;
  int packet;
  uint32_t start = UINT32_MAX - 500;
  ack.begin(start, 10, 3000);
  ack.queued(&packet);
  EXPECT_FALSE(ack.expired(start + 9999)); EXPECT_TRUE(ack.expired(start + 10000));
  ack.started(&packet, start + 9999);
  EXPECT_FALSE(ack.expired(start + 12998)); EXPECT_TRUE(ack.expired(start + 12999));
  ack.cancel(); EXPECT_FALSE(ack.active());
  ack.begin(100, 1234567, 3000); ack.started(nullptr, 200); ack.completed(nullptr);
  EXPECT_TRUE(ack.ready(100));
}

class TraceAck : public SensorIntegration {
protected:
  void SetUp() override { SensorIntegration::SetUp(); mesh.complete_trace_ack_immediately = false; }
  mesh::Packet parse(size_t i) {
    mesh::Packet p{};
    EXPECT_TRUE(mesh.tryParsePacket(&p, radio.sent.at(i).data(), radio.sent.at(i).size()));
    return p;
  }
  void start() { radio.incoming = command("Alice: !hillvue trace"); tick(100); }
  void afterAck() { finish(200); tick(201); }
  uint32_t deadline() {
    return 100 + TraceTransactionBudget::DURATION_MILLIS;
  }
};

TEST_F(TraceAck, RealDispatcherAcknowledgesBeforeProbeAndKeepsNormalOutput) {
  start(); ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace requested, please wait up to 60 seconds.", publicText(0));
  EXPECT_EQ(0u, bot.diagnostics().starts);
  afterAck(); ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(1).getPayloadType());
  finish(301); traceReply(1, 1201, {-24, -32, -16, -8});
  tick(5000); finish(5100);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(2).find("1000 ms RTT (avg SNR)\n1 15 -4.0 dB\n2 14 -6.0 dB"));
  EXPECT_EQ(32, pool.getFreeCount()); EXPECT_EQ(0u, radio.estimates_during_tx);
}

TEST_F(TraceAck, SlowAcknowledgementDoesNotConsumeProbeAdmission) {
  radio.airtime = 10000;
  start(); finish(10100); tick(10101);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(1).getPayloadType());
  EXPECT_EQ(1u, bot.diagnostics().starts); EXPECT_EQ(0u, bot.diagnostics().queue_failures);
  finish(20101);
}

TEST_F(TraceAck, BusyChannelExpiresAckThenGivesProbeItsOwnQueueWindow) {
  mesh.cad_max_duration = 60000;
  radio.busy = true; start();
  tick(10099); EXPECT_TRUE(radio.sent.empty());
  tick(10100); tick(15000); radio.busy = false; tick(15300);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(0).getPayloadType());
  finish(15400);
}

TEST_F(TraceAck, LostAcknowledgementCompletionCannotBlockTrace) {
  start(); mesh.bot = nullptr; finish(200); mesh.bot = &bot;
  tick(1249); EXPECT_EQ(1u, radio.sent.size());
  tick(1250); tick(1251);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(1).getPayloadType()); finish(1351);
}

TEST_F(TraceAck, UncertainFailedAckIsNeverRepeatedAndTracingContinues) {
  mesh.definite_start_failure = false; radio.fail = true; start();
  radio.fail = false; tick(101); tick(102);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(0).getPayloadType()); finish(202);
}

TEST_F(TraceAck, DefiniteAckFailuresRetryOnlyWithinOriginalAdmissionLease) {
  radio.fail = true; start(); tick(350); tick(850); tick(1850); tick(10100);
  radio.fail = false; tick(10101);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(0).getPayloadType()); finish(10201);
}

TEST_F(TraceAck, QueueRejectionRetriesAndHasOneEmittingAcknowledgement) {
  mesh.reject_group = 1; start(); EXPECT_TRUE(radio.sent.empty());
  tick(350); tick(351);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(0).find("Trace requested"));
  finish(451); tick(452); finish(552);
  ASSERT_EQ(2u, radio.sent.size()); EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(1).getPayloadType());
}

TEST_F(TraceAck, PoolExhaustionDoesNotHoldAcknowledgementForever) {
  auto bytes = command("Alice: !hillvue trace");
  std::vector<mesh::Packet*> held;
  while (auto p = mesh.obtainNewPacket()) held.push_back(p);
  // RX needs one slot; the parsed command occupies it during acknowledgement allocation.
  mesh.releasePacket(held.back()); held.pop_back();
  radio.incoming = bytes; tick(100); EXPECT_TRUE(radio.sent.empty());
  tick(10100); tick(10101);
  ASSERT_EQ(1u, radio.sent.size()); EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(0).getPayloadType());
  finish(10201);
  for (auto p : held) mesh.releasePacket(p);
}

TEST_F(TraceAck, CancellingQueuedAckRemovesPleaseWaitAndAllowsAnotherTrace) {
  radio.busy = true; start();
  radio.incoming = command("Bob: !hillvue trace cancel"); tick(200);
  radio.busy = false; tick(3000); finish(3100);
  ASSERT_EQ(1u, radio.sent.size()); EXPECT_EQ("BME680 Sensor: Trace: cancelled", publicText(0));
  radio.incoming = command("Bob: !hillvue trace"); tick(4000);
  ASSERT_EQ(2u, radio.sent.size()); EXPECT_NE(std::string::npos, publicText(1).find("Trace requested"));
}

TEST_F(TraceAck, CancellingTransmittingAckDoesNotFreeDispatchersPacket) {
  start(); EXPECT_EQ(31, pool.getFreeCount());
  radio.incoming = command("Bob: !hillvue trace cancel"); tick(150);
  EXPECT_EQ(31, pool.getFreeCount());  // Half-duplex: cancel is received after TX completes.
  finish(200); tick(3000); finish(3100);
  EXPECT_EQ(32, pool.getFreeCount());
  ASSERT_EQ(2u, radio.sent.size()); EXPECT_EQ("BME680 Sensor: Trace: cancelled", publicText(1));
  EXPECT_EQ(0u, bot.diagnostics().starts);
}

TEST_F(TraceAck, DuplicateAndBusyCommandsNeverRestartAckOrFrozenDeadline) {
  auto original = command("Alice: !hillvue trace"); radio.incoming = original; tick(100);
  radio.incoming = original; tick(150);
  radio.incoming = command("Bob: !hillvue trace"); tick(175);
  finish(200); tick(201); finish(301); tick(3000); finish(3100);
  unsigned acks = 0;
  for (size_t i = 0; i < radio.sent.size(); i++) if (parse(i).getPayloadType() == PAYLOAD_TYPE_GRP_TXT)
    acks += publicText(i).find("Trace requested") != std::string::npos;
  EXPECT_EQ(1u, acks);
}

TEST_F(TraceAck, OverallDeadlineReleasesOwnershipEvenWhenPollingSkipsAllPhases) {
  radio.busy = true; start();
  tick(deadline());
  EXPECT_EQ(32, pool.getFreeCount()); EXPECT_TRUE(radio.sent.empty());
  radio.busy = false; radio.incoming = command("Bob: !hillvue trace"); tick(deadline() + 1);
  ASSERT_EQ(1u, radio.sent.size()); EXPECT_NE(std::string::npos, publicText(0).find("Trace requested"));
}

TEST_F(TraceAck, DeadlineCannotFreeInFlightAckAndLateCompletionCannotRestartIt) {
  start();
  g_mock_millis = deadline(); bot.beforeRadioLoop();
  EXPECT_EQ(31, pool.getFreeCount());
  finish(deadline()); EXPECT_EQ(32, pool.getFreeCount());
  tick(deadline() + 1); EXPECT_EQ(1u, radio.sent.size());
  radio.incoming = command("Bob: !hillvue trace"); tick(deadline() + 2);
  ASSERT_EQ(2u, radio.sent.size()); EXPECT_NE(std::string::npos, publicText(1).find("Trace requested"));
}

TEST_F(TraceAck, ProfileChangeCancelsQueuedAckWithoutSendingItOnNewProfile) {
  radio.busy = true; start(); mesh.profile = 1;
  radio.busy = false; tick(200); tick(3000); finish(3100);
  ASSERT_EQ(1u, radio.sent.size()); EXPECT_EQ("BME680 Sensor: Trace: radio configuration changed", publicText(0));
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TraceAck, NoRepeaterAndUnknownTargetDoNotAcknowledge) {
  radio.incoming = command("Alice: !hillvue trace", 0); tick(100); tick(3000); finish(3100);
  EXPECT_EQ("BME680 Sensor: Trace: no repeaters in request path", publicText(0));
  radio.incoming = command("Bob: !hillvue trace 0000"); tick(4000); tick(7000); finish(7100);
  EXPECT_EQ("BME680 Sensor: Trace: unknown repeater 0000", publicText(1));
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TraceAck, TargetedAcknowledgementPrecedesAuthenticatedDiscovery) {
  auto target = mesh::LocalIdentity(&random);
  uint8_t app[MAX_ADVERT_DATA_SIZE]; auto len = AdvertDataBuilder(ADV_TYPE_REPEATER, "Target").encodeTo(app);
  bot.onAdvertRecv(nullptr, target, 1, app, len);
  char text[60]; snprintf(text, sizeof(text), "Alice: !hillvue trace %02x%02x", target.pub_key[0], target.pub_key[1]);
  radio.incoming = command(text, 2, &channel.channel(1), 2); tick(100);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace requested to Target, please wait up to 60 seconds.", publicText(0, &channel.channel(1)));
  EXPECT_EQ(2, parse(0).getPathHashSize());
  afterAck(); ASSERT_EQ(2u, radio.sent.size()); EXPECT_EQ(PAYLOAD_TYPE_REQ, parse(1).getPayloadType());
  finish(301);
}

TEST_F(TraceAck, StatusDuringAckShowsOverallTimeAndCancelStillWorks) {
  radio.busy = true; start();
  radio.incoming = command("Bob: !hillvue trace status"); tick(200);
  radio.incoming = command("Bob: !hillvue trace cancel"); tick(300);
  radio.busy = false; tick(3000); finish(3100); tick(3101); finish(3201);
  ASSERT_EQ(2u, radio.sent.size());
  bool status = false, cancel = false;
  for (size_t i = 0; i < radio.sent.size(); i++) {
    status |= publicText(i).find("Trace: acknowledging request; 60 s overall left") != std::string::npos;
    cancel |= publicText(i).find("Trace: cancelled") != std::string::npos;
  }
  EXPECT_TRUE(status); EXPECT_TRUE(cancel); EXPECT_EQ(32, pool.getFreeCount());
}

class TraceAckContext : public TraceAck, public ::testing::WithParamInterface<std::tuple<int,int,bool>> {};
TEST_P(TraceAckContext, AcknowledgementUsesFrozenChannelScopeAndWidth) {
  int width = std::get<0>(GetParam()), ch = std::get<1>(GetParam());
  bool scoped = std::get<2>(GetParam());
  auto bytes = command("Alice: !hillvue trace", 2, &channel.channel(ch), width);
  mesh::Packet p{}; ASSERT_TRUE(mesh.tryParsePacket(&p, bytes.data(), bytes.size()));
  TransportKey key{};
  if (scoped) {
    auto region = mesh.regions.putRegion("#ack-scope", 0); region->flags = 0;
    ASSERT_EQ(1, mesh.regions.getTransportKeysFor(*region, &key, 1));
    p.header = (p.header & ~PH_ROUTE_MASK) | ROUTE_TYPE_TRANSPORT_FLOOD;
    p.transport_codes[0] = key.calcTransportCode(&p); p.transport_codes[1] = 0;
    bytes = {p.header};
    auto codes = reinterpret_cast<const uint8_t*>(p.transport_codes);
    bytes.insert(bytes.end(), codes, codes + 4); bytes.push_back(p.path_len);
    bytes.insert(bytes.end(), p.path, p.path + p.getPathByteLen());
    bytes.insert(bytes.end(), p.payload, p.payload + p.payload_len);
  }
  radio.busy = true; radio.incoming = bytes; tick(100);
  mesh.default_scope.key[0] = 0xff;
  radio.busy = false; tick(400);
  ASSERT_EQ(1u, radio.sent.size());
  auto ack = parse(0);
  EXPECT_EQ(width, ack.getPathHashSize());
  EXPECT_EQ(scoped ? ROUTE_TYPE_TRANSPORT_FLOOD : ROUTE_TYPE_FLOOD, ack.getRouteType());
  if (scoped) EXPECT_EQ(key.calcTransportCode(&ack), ack.transport_codes[0]);
  EXPECT_NE(std::string::npos, publicText(0, &channel.channel(ch)).find("Trace requested"));
  finish(500); tick(501); finish(601);
}
INSTANTIATE_TEST_SUITE_P(BothChannelsWidthsAndScopes, TraceAckContext,
    ::testing::Combine(::testing::Values(1,2), ::testing::Values(0,1), ::testing::Bool()));

TEST_F(TraceAck, OverallDeadlineBoundaryAndWrapDoNotExtendAfterStatus) {
  radio.busy = true; mesh.cad_max_duration = UINT32_MAX;
  uint32_t start = UINT32_MAX - 500;
  auto bytes = command("Alice: !hillvue trace");
  // Direct the decoded command through the service so the test isolates its
  // 32-bit deadline from the native host's 64-bit Dispatcher clock.
  mesh::Packet p{}; ASSERT_TRUE(mesh.tryParsePacket(&p, bytes.data(), bytes.size()));
  uint8_t data[MAX_PACKET_PAYLOAD]{};
  auto count = mesh::Utils::MACThenDecrypt(channel.channel().secret, data, p.payload + 1, p.payload_len - 1);
  g_mock_millis = start;
  bot.onGroupDataRecv(&p, PAYLOAD_TYPE_GRP_TXT, channel.channel(), data, count);
  uint32_t end = start + TraceTransactionBudget::DURATION_MILLIS;
  g_mock_millis = end - 1; bot.beforeRadioLoop();
  EXPECT_EQ(TraceReportDelivery::NONE, bot.diagnostics().delivery);
  g_mock_millis = end; bot.beforeRadioLoop();
  EXPECT_EQ(TraceReportDelivery::EXPIRED, bot.diagnostics().delivery);
  EXPECT_EQ(32, pool.getFreeCount());
  g_mock_millis = end + 1; bot.beforeRadioLoop(); EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TraceAck, PrefixIsFrozenEvenWhenAllocationMustRetry) {
  mesh.reject_group = 1; start();
  mesh.node_name = "Renamed Sensor";
  tick(350); tick(351);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace requested, please wait up to 60 seconds.", publicText(0));
  finish(451); tick(452); finish(552);
}

TEST(TraceAckState, OversizedPrefixRejectsBeforeAcceptance) {
  TraceRequestAcknowledgement ack;
  std::string name(160, 'x');
  EXPECT_FALSE(ack.begin(100, 123, 1000, name.c_str()));
  EXPECT_FALSE(ack.active());
  EXPECT_TRUE(ack.begin(100, 123, 1000, "Sensor"));
  EXPECT_STREQ("Sensor: Trace requested, please wait up to 123 seconds.", ack.text());
}

TEST_F(TraceAck, InsufficientRemainingTimeReportsInsteadOfStartingAnotherFullPhase) {
  radio.busy = true; mesh.cad_max_duration = UINT32_MAX; start();
  tick(deadline() - 5000);
  radio.busy = false; tick(deadline() - 2000); finish(deadline() - 1900);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: no result before timeout", publicText(0));
  EXPECT_EQ(0u, bot.diagnostics().starts);
  tick(deadline()); EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TraceAck, WorkStopsAtFiftyFiveSecondsAndReportHasTheFinalFive) {
  radio.busy = true; mesh.cad_max_duration = UINT32_MAX; start();
  g_mock_millis = 55099; bot.beforeRadioLoop();
  EXPECT_TRUE(radio.sent.empty());
  g_mock_millis = 55100; bot.beforeRadioLoop();
  EXPECT_EQ(0u, bot.diagnostics().starts);
  radio.busy = false; tick(55101); finish(55201);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: no result before timeout", publicText(0));
  tick(60099); EXPECT_EQ(32, pool.getFreeCount());
  radio.incoming = command("Bob: !hillvue trace"); tick(60100);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace requested, please wait up to 60 seconds.", publicText(1));
}

TEST_F(TraceAck, QueuedTimeoutReportCannotGetAnotherWindowAfterSixtySeconds) {
  radio.busy = true; mesh.cad_max_duration = UINT32_MAX; start();
  tick(55100);
  EXPECT_EQ(31, pool.getFreeCount());
  tick(60099); EXPECT_EQ(31, pool.getFreeCount());
  tick(60100); EXPECT_EQ(32, pool.getFreeCount());
  radio.busy = false; tick(62000);
  EXPECT_TRUE(radio.sent.empty());
  radio.incoming = command("Bob: !hillvue trace"); tick(62100);
  ASSERT_EQ(1u, radio.sent.size()); EXPECT_NE(std::string::npos, publicText(0).find("up to 60 seconds"));
}

TEST_F(TraceAck, ProbeTimeoutsAreClampedAtTxStartSoSlowRadioStillGetsARetry) {
  radio.airtime = 10000;
  start(); finish(10100); tick(10101); finish(20101);
  tick(32600); tick(35000); finish(45000);
  EXPECT_EQ(2u, bot.diagnostics().starts);
  tick(55100); tick(55101);
  EXPECT_FALSE(bot.diagnostics().has_result);
  g_mock_millis = 60100; bot.beforeRadioLoop();
  // A late TX completion can return only Dispatcher-owned allocation, never
  // re-arm a probe/report or extend the closed operation.
  finish(65200); tick(70000);
  EXPECT_EQ(32, pool.getFreeCount());
  EXPECT_EQ(2u, bot.diagnostics().starts);
}

TEST_F(TraceAck, ProbeReplyInReportReserveCannotReplaceTheTimeoutOrRestartTrace) {
  start(); afterAck(); finish(301);
  radio.busy = true; mesh.cad_max_duration = UINT32_MAX;
  tick(55100);
  traceReply(1, 55101);
  EXPECT_FALSE(bot.diagnostics().has_result);
  radio.busy = false; tick(55400); finish(55500);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: no result before timeout", publicText(2));
  tick(60100); EXPECT_EQ(32, pool.getFreeCount());
}

TEST(TraceBudget, ProbeAndDiscoveryLimitsPreserveShorterAirtimePolicy) {
  uint8_t key[32]{}, via[1]{0x42}; TraceRoute route{};
  ASSERT_EQ(TraceRoute::VALID, TraceRoute::fromOutbound(via, 1, key, route));
  RepeaterTraceProbe probe;
  ASSERT_EQ(RepeaterTraceProbe::STARTED, probe.startRoute(route, 1, 2, 100));
  mesh::Packet p{}; probe.setQueuedPacket(&p); probe.setAirtimeEstimate(10000);
  probe.limitTimeout(1234); ASSERT_TRUE(probe.onTxStarted(&p, 200));
  EXPECT_FALSE(probe.isTimedOut(1433)); EXPECT_TRUE(probe.isTimedOut(1434));
  TraceDiscovery discovery; discovery.begin(key, 100); discovery.queued(&p, 1, 1000);
  discovery.limitTimeout(2000); discovery.started(&p, 200);
  EXPECT_EQ(1000u, discovery.remaining(200));
  discovery.limitTimeout(500); EXPECT_EQ(500u, discovery.remaining(200));
}

TEST(TraceAckState, NamedDestinationPreservesWordsAndCopiesMutableInputs) {
  TraceRequestAcknowledgement ack;
  char name[] = "Sensor", target[] = "Hilltop Repeater";
  ASSERT_TRUE(ack.begin(100, 60, 1000, name, target, "7188"));
  std::string expected = "Sensor: Trace requested to Hilltop Repeater, please wait up to 60 seconds.";
  EXPECT_EQ(expected, ack.text()); name[0] = 'X'; target[0] = 'X';
  int packet; ack.queued(&packet); ack.failed(&packet, 200, true);
  EXPECT_EQ(expected, ack.text()); EXPECT_TRUE(ack.ready(450));
  ack.queued(&packet); ack.failed(&packet, 500, false); EXPECT_FALSE(ack.active());
}

TEST(TraceAckState, NamedMessageExactFitAndOverflowShortenOnlyDestination) {
  TraceRequestAcknowledgement ack;
  std::string tail = ": Trace requested to Hilltop Repeater, please wait up to 60 seconds.";
  std::string name(160 - tail.size(), 'N');
  ASSERT_TRUE(ack.begin(100,60,1000,name.c_str(),"Hilltop Repeater","7188"));
  EXPECT_EQ(160u, strlen(ack.text())); EXPECT_EQ(name + tail, ack.text());
  name += 'N';
  ASSERT_TRUE(ack.begin(100,60,1000,name.c_str(),"Hilltop Repeater","7188"));
  EXPECT_EQ(160u, strlen(ack.text()));
  EXPECT_EQ(name + ": Trace requested to Hilltop Repeate, please wait up to 60 seconds.", ack.text());
  std::string impossible(160, 'N');
  EXPECT_FALSE(ack.begin(100,60,1000,impossible.c_str(),"Hilltop Repeater","7188"));
  EXPECT_FALSE(ack.active());
}

TEST(TraceAckState, NamedLabelUtf8AndFallbackRespectAvailableBytes) {
  TraceRequestAcknowledgement ack;
  std::string overhead = ": Trace requested to , please wait up to 60 seconds.";
  std::string name(160 - overhead.size() - 4, 'N');
  ASSERT_TRUE(ack.begin(100,60,1000,name.c_str(),"\xc3\xa9\xc3\xa9\xc3\xa9","7188"));
  EXPECT_EQ(name + ": Trace requested to \xc3\xa9\xc3\xa9, please wait up to 60 seconds.", ack.text());
  ASSERT_TRUE(ack.begin(100,60,1000,name.c_str(),"\xff","7188"));
  EXPECT_EQ(name + ": Trace requested to 7188, please wait up to 60 seconds.", ack.text());
  ASSERT_TRUE(ack.begin(100,60,1000,"Sensor","","7188"));
  EXPECT_STREQ("Sensor: Trace requested to 7188, please wait up to 60 seconds.",ack.text());
  name += 'N';
  EXPECT_FALSE(ack.begin(100,60,1000,name.c_str(),"\xf0\x9f\x8f\x94","7188"));
  EXPECT_FALSE(ack.active());
}

int main(int argc, char** argv) { ::testing::InitGoogleTest(&argc, argv); return RUN_ALL_TESTS(); }
