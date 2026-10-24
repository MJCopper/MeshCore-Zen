#include "../sensor_mocks/SensorIntegrationFixture.h"
#include <cctype>
#include <tuple>

class TargetTrace : public SensorIntegration {
protected:
  TestRadio remote_radio;
  StaticPoolPacketManager remote_pool{32};
  SimpleMeshTables remote_tables;
  TestMesh remote{remote_radio, clock, random, rtc, remote_pool, remote_tables};
  mesh::LocalIdentity target;
  TransportKey scope{};

  void advertise(const mesh::Identity& id, const char* name = "Target") {
    uint8_t app[MAX_ADVERT_DATA_SIZE];
    auto length = AdvertDataBuilder(ADV_TYPE_REPEATER, name).encodeTo(app);
    bot.onAdvertRecv(nullptr, id, 1, app, length);
  }
  void SetUp() override {
    SensorIntegration::SetUp();
    mesh.self_id = mesh::LocalIdentity(&random);
    target = mesh::LocalIdentity(&random);
    remote.self_id = target;
    remote.begin();
    advertise(target);
  }
  mesh::Packet parse(const std::vector<uint8_t>& bytes) {
    mesh::Packet packet{};
    EXPECT_TRUE(mesh.tryParsePacket(&packet, bytes.data(), bytes.size()));
    return packet;
  }
  std::vector<uint8_t> wire(const mesh::Packet& packet) {
    std::vector<uint8_t> bytes{packet.header};
    if (packet.hasTransportCodes()) {
      auto codes = reinterpret_cast<const uint8_t*>(packet.transport_codes);
      bytes.insert(bytes.end(), codes, codes + 4);
    }
    bytes.push_back(packet.path_len);
    bytes.insert(bytes.end(), packet.path, packet.path + packet.getPathByteLen());
    bytes.insert(bytes.end(), packet.payload, packet.payload + packet.payload_len);
    return bytes;
  }
  std::string text(const mesh::Identity& id, bool upper = false) {
    char buffer[40];
    snprintf(buffer, sizeof(buffer), "Alice: !hillvue trace %02x%02x", id.pub_key[0], id.pub_key[1]);
    std::string result(buffer);
    if (upper) for (auto& c : result) c = toupper((unsigned char)c);
    return result;
  }
  void begin(uint8_t width = 1, uint8_t channel_index = 0, bool upper = false) {
    auto packet = parse(command(text(target, upper).c_str(), 2, &channel.channel(channel_index), width));
    if (!scope.isNull()) {
      packet.header = (packet.header & ~PH_ROUTE_MASK) | ROUTE_TYPE_TRANSPORT_FLOOD;
      packet.transport_codes[0] = scope.calcTransportCode(&packet); packet.transport_codes[1] = 0;
    }
    radio.incoming = wire(packet);
    tick(100);
  }
  uint32_t requestTag(size_t index = 0, const mesh::LocalIdentity* identity = nullptr) {
    auto packet = parse(radio.sent.at(index));
    EXPECT_EQ(PAYLOAD_TYPE_REQ, packet.getPayloadType());
    uint8_t secret[32], data[MAX_PACKET_PAYLOAD];
    (identity ? *identity : target).calcSharedSecret(secret, mesh.self_id);
    int length = mesh::Utils::MACThenDecrypt(secret, data, packet.payload + 2, packet.payload_len - 2);
    EXPECT_GE(length, 13);
    EXPECT_EQ(3, data[4]); EXPECT_EQ((uint8_t)~1, data[5]);
    uint32_t tag; memcpy(&tag, data, 4); return tag;
  }
  std::vector<uint8_t> pathReply(uint32_t tag, const std::vector<uint8_t>& via = {},
                                 uint8_t width = 1, uint8_t extra_type = PAYLOAD_TYPE_RESPONSE,
                                 const mesh::LocalIdentity* identity = nullptr) {
    remote.self_id = identity ? *identity : target;
    uint8_t secret[32]; remote.self_id.calcSharedSecret(secret, mesh.self_id);
    uint8_t extra[5]{}; memcpy(extra, &tag, 4); extra[4] = 0xaa;
    auto packet = remote.createPathReturn(mesh.self_id, secret, via.data(),
          ((width - 1) << 6) | (via.size() / width), extra_type, extra, sizeof(extra));
    EXPECT_NE(nullptr, packet);
    packet->header = (packet->header & ~PH_ROUTE_MASK) | ROUTE_TYPE_FLOOD;
    packet->setPathHashSizeAndCount(width, 0);
    if (!scope.isNull()) {
      packet->header = (packet->header & ~PH_ROUTE_MASK) | ROUTE_TYPE_TRANSPORT_FLOOD;
      packet->transport_codes[0] = scope.calcTransportCode(packet); packet->transport_codes[1] = 0;
    }
    auto result = wire(*packet); remote.releasePacket(packet); return result;
  }
  void discover(uint32_t now = 500, std::vector<uint8_t> via = {}, uint8_t width = 1) {
    radio.incoming = pathReply(requestTag(), via, width); tick(now);
  }
  ClientInfo* addAdmin() {
    auto client = mesh.management_acl.putClient(target, PERM_ACL_ADMIN);
    mesh.self_id.calcSharedSecret(client->shared_secret, target);
    client->out_path_len = 1; client->out_path[0] = 0x99;
    return client;
  }
};

TEST_F(TargetTrace, AuthenticatedDiscoveryUsesRtcAndDirectTargetLoopThenNormalReport) {
  begin();
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_GE(requestTag(), 1800000000u);
  finish(200); discover();
  ASSERT_EQ(2u, radio.sent.size());
  auto trace = parse(radio.sent[1]);
  EXPECT_EQ(PAYLOAD_TYPE_TRACE, trace.getPayloadType());
  EXPECT_EQ(target.pub_key[0], trace.payload[9]);
  EXPECT_EQ(10, trace.payload_len);
  finish(600); traceReply(1, 1500, {-24, -8}); tick(5000); finish(5100);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: 1000 ms RTT (avg SNR)\n1 Target -4.0 dB", publicText(2));
  EXPECT_EQ(0, mesh.management_acl.getNumClients());
  EXPECT_EQ(32, pool.getFreeCount()); EXPECT_EQ(0u, fs.writes);
}

TEST_F(TargetTrace, UppercasePrefixSelectsTheSameFullIdentity) {
  begin(1, 0, true);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_GE(requestTag(), 1800000000u);
}

TEST_F(TargetTrace, UnknownPrefixReportsWithoutDiscovery) {
  radio.incoming = command("Alice: !hillvue trace 0000"); tick(100); tick(3000);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: unknown repeater 0000", publicText(0));
}

TEST_F(TargetTrace, AmbiguousPrefixNeverSelectsTheFirstCacheEntry) {
  auto other = target; other.pub_key[31] ^= 1; advertise(other, "Other");
  begin(); tick(3000);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(0).find("Trace: ambiguous repeater"));
}

TEST_F(TargetTrace, DiscoveryTimeoutIsBoundedAndNotAnUnreachableClaim) {
  begin(); finish(200); tick(13900); tick(13901); finish(14000);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_GT(requestTag(1), requestTag(0));
  tick(27700); tick(30000); finish(30100); tick(120000);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: discovery timed out", publicText(2));
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TargetTrace, NoReplyIsAcceptedUntilItsDiscoveryActuallyTransmitted) {
  radio.busy = true; begin();
  EXPECT_TRUE(radio.sent.empty());
  radio.incoming = pathReply(1800000001); tick(300);
  EXPECT_TRUE(radio.sent.empty());
  tick(10100); tick(20100); radio.busy = false; tick(24000); finish(24100);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: discovery radio busy", publicText(0));
}

TEST_F(TargetTrace, TwoRejectedStartsCannotCreateAnUnlimitedDiscoveryStream) {
  radio.fail = true; begin(); tick(200);
  EXPECT_EQ(2u, radio.start_calls);
  radio.fail = false; tick(3000); finish(3100); tick(60000);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: discovery transmit failed", publicText(0));
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TargetTrace, WrongTimestampAndTamperedAuthenticationCannotStartTrace) {
  begin(); auto tag = requestTag(); finish(200);
  radio.incoming = pathReply(tag + 1); tick(300);
  radio.incoming = pathReply(tag); radio.incoming.back() ^= 1; tick(400);
  EXPECT_EQ(1u, radio.sent.size());
  discover(500);
  EXPECT_EQ(2u, radio.sent.size());
}

TEST_F(TargetTrace, DiscoveryDoesNotInstallOrChangeAnExistingAdminRoute) {
  auto client = addAdmin(); begin(); finish(200); discover();
  EXPECT_EQ(0u, mesh.management_path_updates);
  EXPECT_EQ(1, client->out_path_len); EXPECT_EQ(0x99, client->out_path[0]);
  EXPECT_EQ(PERM_ACL_ADMIN, client->permissions);
}

TEST_F(TargetTrace, UnownedResponseCannotInstallAdminRouteButNormalPathStillCan) {
  auto client = addAdmin();
  radio.incoming = pathReply(123, {0x12}); tick(100);
  EXPECT_EQ(0u, mesh.management_path_updates); EXPECT_EQ(0x99, client->out_path[0]);
  radio.incoming = pathReply(124, {0x34}, 1, 0); tick(200);
  EXPECT_EQ(1u, mesh.management_path_updates); EXPECT_EQ(0x34, client->out_path[0]);
}

TEST_F(TargetTrace, LatePathAfterCompletionCannotMutateAdminRouteOrSendMoreTraffic) {
  auto client = addAdmin(); begin(); auto tag = requestTag(); finish(200); discover();
  finish(600); traceReply(1, 1500); tick(5000); finish(5100);
  radio.incoming = pathReply(tag, {0x77}); tick(6000); tick(20000);
  EXPECT_EQ(3u, radio.sent.size()); EXPECT_EQ(0x99, client->out_path[0]);
  EXPECT_EQ(0u, mesh.management_path_updates); EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TargetTrace, RadioChangeCancelsQueuedDiscoveryBeforeDispatcherCanSendIt) {
  radio.busy = true; begin(); mesh.profile = 1;
  radio.busy = false; tick(200); tick(3000); finish(3100); tick(60000);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: radio configuration changed", publicText(0));
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TargetTrace, OversizedOrUnsupportedDiscoveredPathsFailWithoutProbing) {
  begin(); finish(200);
  radio.incoming = pathReply(requestTag(), std::vector<uint8_t>(10, 0x44)); tick(500); tick(3000); finish(3100);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: path too long (max 10 repeaters)", publicText(1));
}

TEST_F(TargetTrace, UnsupportedThreeByteHashPathIsRejected) {
  begin(); finish(200);
  radio.incoming = pathReply(requestTag(), {}, 3); tick(500); tick(3000); finish(3100);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: discovered path unsupported", publicText(1));
}

TEST_F(TargetTrace, UnsupportedRequestHashWidthDoesNotSendDiscovery) {
  begin(3); tick(3000); finish(3100);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: unsupported path hash width", publicText(0));
}

TEST_F(TargetTrace, AllocationFailureRecoversWithinDiscoveryAdmissionBudget) {
  std::vector<mesh::Packet*> held;
  while (pool.getFreeCount() > 1) held.push_back(pool.allocNew());
  begin(); EXPECT_TRUE(radio.sent.empty());
  for (auto packet : held) pool.free(packet);
  tick(1000); tick(1001);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ(PAYLOAD_TYPE_REQ, parse(radio.sent[0]).getPayloadType());
}

TEST_F(TargetTrace, BusyNoticeDuringDiscoveryDoesNotChangeItsTarget) {
  begin(); finish(200);
  radio.incoming = command("Bob: !hillvue trace", 2, &channel.channel(1)); tick(300); tick(3000); finish(3100);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(1, &channel.channel(1)).find("already running"));
  discover(3200); finish(3300); traceReply(2, 4000); tick(8000); finish(8100);
  ASSERT_EQ(4u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(3).find("1 Target"));
}


TEST_F(TargetTrace, ParserRejectsMalformedDestinationsAndCombinations) {
  for (const char* argument : {"trace 123", "trace 12345", "trace 0x12", "trace 4dg5",
                               "trace 4df5 ping", "trace t", "trace4df5", "trace -123"}) {
    radio.incoming = command((std::string("Alice: !hillvue ") + argument).c_str());
    tick(g_mock_millis + 100);
  }
  tick(5000);
  EXPECT_TRUE(radio.sent.empty());
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TargetTrace, SameHashDifferentFullKeyCannotAuthenticateDiscovery) {
  begin(); auto tag = requestTag(); finish(200);
  auto impostor = mesh::LocalIdentity(&random);
  impostor.pub_key[0] = target.pub_key[0]; // same peer search hash, different shared secret
  radio.incoming = pathReply(tag, {}, 1, PAYLOAD_TYPE_RESPONSE, &impostor);
  tick(300); EXPECT_EQ(1u, radio.sent.size());
  discover(500); EXPECT_EQ(2u, radio.sent.size());
}

TEST_F(TargetTrace, FullCollidingAclSearchStillHasBoundedTransientDiscoverySlot) {
  for (uint8_t i = 1; i <= 24; i++) {
    auto key = target; key.pub_key[31] ^= i;
    auto client = mesh.management_acl.putClient(key, PERM_ACL_ADMIN);
    ASSERT_NE(nullptr, client);
    mesh.self_id.calcSharedSecret(client->shared_secret, key);
  }
  begin(); finish(200); discover();
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(radio.sent[1]).getPayloadType());
  EXPECT_EQ(24, mesh.management_acl.getNumClients());
  EXPECT_EQ(0u, mesh.management_path_updates);
}

TEST_F(TargetTrace, DiscoveryIgnoresWrongScopeAndUsesFrozenScopeForItsReport) {
  auto region = mesh.regions.putRegion("#target-scope", 0); region->flags = 0;
  ASSERT_EQ(1, mesh.regions.getTransportKeysFor(*region, &scope, 1));
  begin(2, 1); auto tag = requestTag(); finish(200);
  auto wrong = parse(pathReply(tag));
  wrong.transport_codes[0] ^= 1;
  radio.incoming = wire(wrong); tick(300);
  EXPECT_EQ(1u, radio.sent.size());
  // Different authenticated PATH payload avoids the mesh's first-packet duplicate cache.
  radio.incoming = pathReply(tag, {0x10, 0x11}, 2); tick(500); finish(600);
  ASSERT_EQ(2u, radio.sent.size());
  mesh.default_scope.key[0] = 0xff;
  traceReply(1, 1500); tick(5000); finish(5100);
  ASSERT_EQ(3u, radio.sent.size());
  auto report = parse(radio.sent[2]);
  EXPECT_EQ(ROUTE_TYPE_TRANSPORT_FLOOD, report.getRouteType());
  EXPECT_EQ(scope.calcTransportCode(&report), report.transport_codes[0]);
  EXPECT_EQ(2, report.getPathHashSize());
  EXPECT_NE(std::string::npos, publicText(2, &channel.channel(1)).find("avg SNR"));
}

TEST_F(TargetTrace, LateFirstDiscoveryCanCancelAQueuedDiscoveryRetry) {
  begin(); auto tag = requestTag(); finish(200);
  radio.busy = true; tick(30100);
  radio.incoming = pathReply(tag); tick(30200);
  radio.busy = false; tick(31000);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(radio.sent[1]).getPayloadType());
  finish(31100); traceReply(1, 32000); tick(36000); finish(36100);
  EXPECT_EQ(3u, radio.sent.size()); EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TargetTrace, FreshDiscoveryAlternativesCanSupplyTheTraceRetry) {
  begin(); auto tag = requestTag(); finish(200);
  discover(500, {0x10}); finish(600);
  radio.incoming = pathReply(tag, {0x20}); tick(700);
  tick(10500); tick(12600); finish(12700);
  ASSERT_EQ(3u, radio.sent.size());
  auto retry = parse(radio.sent[2]);
  EXPECT_EQ(0x20, retry.payload[9]); EXPECT_EQ(target.pub_key[0], retry.payload[10]);
  traceReply(2, 14000, {-24, 8, -8, 0}); tick(18000); finish(18100);
  ASSERT_EQ(4u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(3).find("2 Target +0.0 dB"));
}

TEST_F(TargetTrace, TargetedFallbackRetainsOriginalDiscoveredRouteAndPairedSnr) {
  begin(); auto tag = requestTag(); finish(200);
  discover(500, {0x10}); finish(600);
  radio.incoming = pathReply(tag, {0x20}); tick(700);
  tick(10500); tick(12600); finish(12700);
  traceReply(1, 14000, {-24, -32, -16, -8});
  tick(22600); tick(26000); finish(26100);
  ASSERT_EQ(4u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(3).find("1 10 -4.0 dB\n2 Target -6.0 dB"));
  EXPECT_EQ(0, bot.diagnostics().selected.attempt);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TargetTrace, OldTargetResponseCannotCompleteANewerTargetTransaction) {
  begin(); auto old_tag = requestTag(); auto old_target = target; finish(200);
  mesh.profile = 1; tick(300); tick(3000); finish(3100);
  target = mesh::LocalIdentity(&random); advertise(target, "New Target");
  radio.incoming = command(text(target).c_str()); tick(4000); finish(4100);
  ASSERT_EQ(3u, radio.sent.size());
  radio.incoming = pathReply(old_tag, {}, 1, PAYLOAD_TYPE_RESPONSE, &old_target); tick(4200);
  EXPECT_EQ(3u, radio.sent.size());
  radio.incoming = pathReply(requestTag(2)); tick(4300);
  EXPECT_EQ(4u, radio.sent.size());
}

TEST_F(TargetTrace, AuthenticatedMalformedPathCannotReadBeyondItsDecryptedPayload) {
  addAdmin(); begin(); finish(200);
  uint8_t secret[32]; target.calcSharedSecret(secret, mesh.self_id);
  mesh::Packet packet{};
  packet.header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_PATH << PH_TYPE_SHIFT);
  packet.payload[0] = mesh.self_id.pub_key[0]; packet.payload[1] = target.pub_key[0];
  uint8_t malformed[2] = {uint8_t((2 << 6) | 20), PAYLOAD_TYPE_RESPONSE};
  packet.payload_len = 2 + mesh::Utils::encryptThenMAC(secret, packet.payload + 2, malformed, sizeof(malformed));
  radio.incoming = wire(packet); tick(300);
  EXPECT_EQ(1u, radio.sent.size()); EXPECT_EQ(0u, mesh.management_path_updates);
  discover(500); EXPECT_EQ(2u, radio.sent.size());
}

TEST_F(TargetTrace, OtherAuthenticatedAdminTrafficKeepsItsNormalOwner) {
  addAdmin(); begin(); finish(200);
  uint8_t secret[32], data[5]{}; target.calcSharedSecret(secret, mesh.self_id);
  data[4] = 1;
  auto packet = remote.createDatagram(PAYLOAD_TYPE_REQ, mesh.self_id, secret, data, sizeof(data));
  ASSERT_NE(nullptr, packet);
  packet->header = (packet->header & ~PH_ROUTE_MASK) | ROUTE_TYPE_DIRECT;
  packet->path_len = 0;
  radio.incoming = wire(*packet); remote.releasePacket(packet); tick(300);
  EXPECT_EQ(1u, mesh.management_data_calls);
  EXPECT_EQ(1u, radio.sent.size());
}

TEST(TargetDiscoveryState, TimersAndOwnershipWorkAcrossMillisWrap) {
  TraceDiscovery discovery;
  uint8_t key[32]{}; key[0] = 0x42;
  mesh::Packet packet;
  uint32_t now = UINT32_MAX - 100;
  discovery.begin(key, now); discovery.queued(&packet, 123, 30000);
  EXPECT_FALSE(discovery.owns(key, 123));
  discovery.started(&packet, now + 50); discovery.completed(&packet);
  EXPECT_TRUE(discovery.owns(key, 123));
  discovery.tick(now + 30049); EXPECT_EQ(TraceDiscovery::WAIT, discovery.phase());
  discovery.tick(now + 30050); EXPECT_EQ(TraceDiscovery::QUEUE, discovery.phase());
  discovery.cancel();
  EXPECT_TRUE(discovery.owns(key, 123)); EXPECT_FALSE(discovery.current(key, 123));
}

TEST(TargetHistory, FullTargetIdentitySeparatesBareAndTargetedRouteEvidence) {
  TraceRouteHistory history;
  uint8_t a[32]{}, b[32]{}; a[0] = b[0] = 0x42; b[31] = 1;
  TraceRoute route{};
  ASSERT_EQ(TraceRoute::VALID, TraceRoute::fromOutbound(NULL, 0, a, route));
  history.record(route, 0, 0, 100, true, 10, {}, a);
  EXPECT_EQ(2, history.rank(route, 0, 0, 101, {}, a));
  EXPECT_EQ(1, history.rank(route, 0, 0, 101, {}, b));
  EXPECT_EQ(1, history.rank(route, 0, 0, 101));
  history.record(route, 0, 0, 102, false);
  EXPECT_EQ(2, history.rank(route, 0, 0, 103, {}, a));
  EXPECT_EQ(0, history.rank(route, 0, 0, 103));
}

TEST_F(TargetTrace, ExpiringDiscoveryKeepsOwnershipUntilItsFailureIsHandled) {
  radio.busy = true; begin(); tick(10100);
  radio.incoming = command("Bob: !hillvue trace");
  radio.busy = false; tick(20100);
  // The old operation must not admit and then cancel a new bare probe.
  EXPECT_TRUE(radio.sent.empty());
  EXPECT_EQ(0u, bot.diagnostics().starts);
}

TEST_F(TargetTrace, MissingReportCompletionCannotLeaveAllFutureTracesBusy) {
  begin(); finish(200); discover(); finish(600); traceReply(1, 1500);
  tick(5000);
  ASSERT_EQ(3u, radio.sent.size());
  mesh.bot = nullptr; finish(5100); mesh.bot = &bot;
  EXPECT_EQ(32, pool.getFreeCount());  // Dispatcher released it; owner notification was lost.
  tick(60000);
  EXPECT_EQ(TraceReportDelivery::UNCERTAIN, bot.diagnostics().delivery);
  EXPECT_EQ(1u, bot.diagnostics().report_failures);
  radio.incoming = command("Bob: !hillvue trace"); tick(60100);
  ASSERT_EQ(4u, radio.sent.size());
  EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(radio.sent.back()).getPayloadType());
}

TEST_F(TargetTrace, ThirtySecondBusyReplyExplainsDiscoveryAndStatusShowsItsAttempt) {
  begin(); finish(200);
  radio.incoming = command("Bob: !hillvue trace"); tick(30000);
  tick(30100); tick(30101); finish(30200); tick(33000); finish(33100);
  bool found = false;
  for (size_t i = 0; i < radio.sent.size(); i++) if (parse(radio.sent[i]).getPayloadType() == PAYLOAD_TYPE_GRP_TXT) {
    auto text = publicText(i);
    if (text.find("already running; discovery") != std::string::npos) found = true;
  }
  EXPECT_TRUE(found);
  radio.incoming = command("Bob: !hillvue trace status"); tick(34000); tick(37000); finish(37100);
  EXPECT_NE(std::string::npos, publicText(radio.sent.size() - 1).find("Trace: discovery 2/2"));
}

TEST_F(TargetTrace, CancellingQueuedDiscoveryUnblocksANewBareTraceWithoutReboot) {
  radio.busy = true; begin();
  radio.incoming = command("Bob: !hillvue trace cancel"); tick(200);
  radio.busy = false; tick(3000); finish(3100);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: cancelled", publicText(0));
  radio.incoming = command("Bob: !hillvue trace"); tick(4000); finish(4100);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(radio.sent[1]).getPayloadType());
  traceReply(1, 4500); tick(8000); finish(8100);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TargetTrace, CancelAndRestartDiscoveryIgnoreTheCancelledOperationsLateReply) {
  begin(); auto old_tag = requestTag(); finish(200);
  radio.incoming = command("Bob: !hillvue trace cancel"); tick(300); tick(3000); finish(3100);
  ASSERT_EQ(2u, radio.sent.size()); EXPECT_EQ("BME680 Sensor: Trace: cancelled", publicText(1));
  radio.incoming = command(text(target).c_str()); tick(4000); finish(4100);
  ASSERT_EQ(3u, radio.sent.size());
  auto new_tag = requestTag(2); EXPECT_GT(new_tag, old_tag);
  radio.incoming = pathReply(old_tag, {0x10}); tick(4200); EXPECT_EQ(3u, radio.sent.size());
  radio.incoming = pathReply(new_tag); tick(4500); finish(4600);
  ASSERT_EQ(4u, radio.sent.size());
  traceReply(3, 5000); tick(9000); finish(9100);
  EXPECT_EQ(5u, radio.sent.size()); EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TargetTrace, CancelAndIdleStatusDoNotStartDiscovery) {
  radio.incoming = command("Bob: !hillvue trace status"); tick(100); tick(3000); finish(3100);
  radio.incoming = command("Bob: !hillvue trace cancel"); tick(4000); tick(7000); finish(7100);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: idle", publicText(0));
  EXPECT_EQ("BME680 Sensor: Trace: idle", publicText(1));
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TargetTrace, CancellingQueuedResultRemovesItAndFreesTheTraceSlot) {
  begin(); finish(200); discover(); finish(600); traceReply(1, 1500);
  radio.incoming = command("Bob: !hillvue trace cancel"); tick(1600); tick(5000); finish(5100);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: cancelled", publicText(2));
  radio.incoming = command("Bob: !hillvue trace"); tick(6000);
  ASSERT_EQ(4u, radio.sent.size()); EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(radio.sent[3]).getPayloadType());
}

TEST_F(TargetTrace, BareTraceCanBeCancelledAndOldResultsCannotRestartIt) {
  radio.incoming = command("Bob: !hillvue trace"); tick(100); finish(200);
  radio.incoming = command("Bob: !hillvue trace cancel"); tick(300); tick(3000); finish(3100);
  ASSERT_EQ(2u, radio.sent.size()); EXPECT_EQ("BME680 Sensor: Trace: cancelled", publicText(1));
  traceReply(0, 3200); tick(10000); EXPECT_EQ(2u, radio.sent.size());
  radio.incoming = command(text(target).c_str()); tick(11000);
  ASSERT_EQ(3u, radio.sent.size()); EXPECT_EQ(PAYLOAD_TYPE_REQ, parse(radio.sent[2]).getPayloadType());
}

TEST_F(TargetTrace, DiscoveryFailureEventuallyAdmitsANewTrace) {
  begin(); finish(200); tick(13900); tick(13901); finish(14000);
  tick(27700); tick(30000); finish(30100);
  radio.incoming = command("Bob: !hillvue trace"); tick(64000);
  ASSERT_EQ(4u, radio.sent.size());
  EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(radio.sent[3]).getPayloadType());
}

TEST(TargetDiscoveryState, NullCallbacksDoNotAdvanceAnUnsubmittedDiscovery) {
  TraceDiscovery discovery; uint8_t key[32]{};
  discovery.begin(key, 100);
  discovery.started(NULL, 200); discovery.failed(NULL, 300);
  EXPECT_EQ(TraceDiscovery::QUEUE, discovery.phase());
  EXPECT_EQ(1, discovery.attempt()); EXPECT_FALSE(discovery.owns(key, 0));
}

TEST_F(TargetTrace, SlowRadioDiscoveryBudgetNeverAdvertisesMoreThanSixtySeconds) {
  radio.airtime = 1512;  // RadioLib max-frame airtime at shipped SF8/BW62.5/CR5 defaults.
  begin(); finish(200);
  radio.incoming = command("Bob: !hillvue trace status"); tick(30000); tick(33000); finish(33100);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(1).find("BME680 Sensor: Trace: discovery 2/2 queued"));
  EXPECT_NE(std::string::npos, publicText(1).find("s overall left"));
  radio.incoming = command("Bob: !hillvue trace cancel"); tick(34000); tick(37000); finish(37100);
  EXPECT_EQ("BME680 Sensor: Trace: cancelled", publicText(2));
  radio.incoming = command("Bob: !hillvue trace"); tick(40000);
  ASSERT_EQ(4u, radio.sent.size()); EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(radio.sent[3]).getPayloadType());
}

TEST_F(TargetTrace, CancelReplyUsesTheOperatorsChannelAndScopeRatherThanTheOldTraceScope) {
  auto region = mesh.regions.putRegion("#trace-scope", 0); region->flags = 0;
  ASSERT_EQ(1, mesh.regions.getTransportKeysFor(*region, &scope, 1));
  begin(); auto old_tag = requestTag(); finish(200);
  TransportKey other_scope{};
  region = mesh.regions.putRegion("#operator-scope", 0); region->flags = 0;
  ASSERT_EQ(1, mesh.regions.getTransportKeysFor(*region, &other_scope, 1));
  auto cancel = parse(command("Bob: !hillvue trace cancel", 2, &channel.channel(1)));
  cancel.header = (cancel.header & ~PH_ROUTE_MASK) | ROUTE_TYPE_TRANSPORT_FLOOD;
  cancel.transport_codes[0] = other_scope.calcTransportCode(&cancel); cancel.transport_codes[1] = 0;
  radio.incoming = wire(cancel); tick(300); tick(3000); finish(3100);
  ASSERT_EQ(2u, radio.sent.size());
  auto ack = parse(radio.sent[1]);
  EXPECT_EQ(other_scope.calcTransportCode(&ack), ack.transport_codes[0]);
  EXPECT_EQ("BME680 Sensor: Trace: cancelled", publicText(1, &channel.channel(1)));
  radio.incoming = pathReply(old_tag); tick(3200); tick(6000);
  EXPECT_EQ(2u, radio.sent.size()); EXPECT_EQ(32, pool.getFreeCount());
}

class TraceTerminalRecovery : public TargetTrace, public ::testing::WithParamInterface<int> {};
TEST_P(TraceTerminalRecovery, EveryTerminalPathAllowsAnotherTraceAndReturnsAllPoolSlots) {
  uint32_t next_at = 0;
  switch (GetParam()) {
    case 0:
      begin(); finish(200); discover(); finish(600); traceReply(1, 1500);
      tick(5000); finish(5100); next_at = 6000; break;
    case 1:
      begin(); finish(200); tick(13900); tick(13901); finish(14000);
      tick(27700); tick(30000); finish(30100); next_at = 64000; break;
    case 2:
      radio.busy = true; begin(); tick(10100); tick(20100);
      radio.busy = false; tick(24000); finish(24100); next_at = 25000; break;
    case 3:
      radio.fail = true; begin(); tick(200); radio.fail = false;
      tick(3000); finish(3100); next_at = 4000; break;
    case 4:
      begin(); finish(200); discover(); finish(600);
      tick(9000); tick(12000); finish(12100); tick(20000); tick(23000);
      tick(26000); finish(26100); next_at = 27000; break;
    case 5:
      begin(); finish(200); discover(); finish(600); traceReply(1, 1500);
      radio.busy = true; tick(15000); radio.busy = false; next_at = 16000; break;
  }
  ASSERT_EQ(32, pool.getFreeCount());
  size_t previous = radio.sent.size();
  radio.incoming = command("Next: !hillvue trace"); tick(next_at);
  ASSERT_EQ(previous + 1, radio.sent.size());
  ASSERT_EQ(PAYLOAD_TYPE_TRACE, parse(radio.sent.back()).getPayloadType());
  finish(next_at + 100); traceReply(previous, next_at + 500);
  tick(next_at + 4000); finish(next_at + 4100);
  EXPECT_EQ(32, pool.getFreeCount());
}
INSTANTIATE_TEST_SUITE_P(SuccessAndAllFailureStages, TraceTerminalRecovery, ::testing::Range(0, 6));

TEST_F(TargetTrace, DelayedCompletionPollDoesNotDiscardTheSecondReportPart) {
  begin(); finish(200);
  discover(500, {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38});
  finish(600); traceReply(1, 1500); tick(5000);
  ASSERT_EQ(3u, radio.sent.size()); EXPECT_NE(std::string::npos, publicText(2).find("1/2"));
  finish(7000);  // polling was delayed beyond airtime + watchdog grace, but TX completed
  tick(10000); tick(10001); finish(10100);
  ASSERT_EQ(4u, radio.sent.size()); EXPECT_NE(std::string::npos, publicText(3).find("2/2"));
  EXPECT_EQ(TraceReportDelivery::DELIVERED, bot.diagnostics().delivery);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TargetTrace, TxStartObserverUsesCachedBudgetInsteadOfQueryingTheTransmittingRadio) {
  begin(); finish(200); discover(); finish(600); traceReply(1, 1500);
  tick(5000); finish(5100);
  EXPECT_EQ(0u, radio.estimates_during_tx);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(TargetTrace, BareQueuedProbeCannotStartOnAChangedRadioProfile) {
  radio.busy = true; radio.incoming = command("Bob: !hillvue trace"); tick(100);
  mesh.profile = 1; radio.busy = false; tick(200); tick(3000); finish(3100);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: radio configuration changed", publicText(0));
  radio.incoming = command("Next: !hillvue trace"); tick(4000);
  ASSERT_EQ(2u, radio.sent.size()); EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(radio.sent[1]).getPayloadType());
}

TEST_F(TargetTrace, QueuedResultIsNotSentUsingAChangedRadioProfileAndOldWatchdogBudget) {
  begin(); finish(200); discover(); finish(600); traceReply(1, 1500);
  mesh.profile = 1; radio.airtime = 2000; tick(1600); tick(6000);
  EXPECT_EQ(2u, radio.sent.size()); EXPECT_EQ(TraceReportDelivery::UNCERTAIN, bot.diagnostics().delivery);
  radio.incoming = command("Next: !hillvue trace"); tick(7000);
  ASSERT_EQ(3u, radio.sent.size()); EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(radio.sent[2]).getPayloadType());
}

TEST_F(TargetTrace, RadioChangeDuringResultTransmissionCannotQueueAContradictoryFailure) {
  begin(); finish(200); discover(); finish(600); traceReply(1, 1500); tick(5000);
  mesh.profile = 1; finish(5100); tick(9000);
  ASSERT_EQ(3u, radio.sent.size()); EXPECT_EQ(TraceReportDelivery::UNCERTAIN, bot.diagnostics().delivery);
  radio.incoming = command("Next: !hillvue trace"); tick(10000);
  ASSERT_EQ(4u, radio.sent.size()); EXPECT_EQ(PAYLOAD_TYPE_TRACE, parse(radio.sent[3]).getPayloadType());
}

TEST_F(TargetTrace, DiscoveryAtWorkCutoffReportsItsStageAndReleasesBySixtySeconds) {
  mesh.complete_trace_ack_immediately = false;
  radio.busy = true; mesh.cad_max_duration = UINT32_MAX;
  begin();
  tick(55100);
  radio.busy = false; tick(55400); finish(55500);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: discovery timed out", publicText(0));
  EXPECT_EQ(0u, bot.diagnostics().starts);
  radio.incoming = command("Bob: !hillvue trace"); tick(60100);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace requested, please wait up to 60 seconds.", publicText(1));
}

class TargetRoutes : public TargetTrace, public ::testing::WithParamInterface<std::tuple<int, int>> {};
TEST_P(TargetRoutes, DiscoveredRouteIncludesTargetAndBothWidthsThroughTenRepeaters) {
  int width = std::get<0>(GetParam()), hops = std::get<1>(GetParam());
  begin(width); finish(200);
  std::vector<uint8_t> via((hops - 1) * width);
  for (size_t i = 0; i < via.size(); i++) via[i] = 0x30 + i;
  discover(500, via, width);
  ASSERT_EQ(2u, radio.sent.size());
  auto trace = parse(radio.sent[1]);
  EXPECT_EQ(width - 1, trace.payload[8]);
  EXPECT_EQ(9 + (2 * hops - 1) * width, trace.payload_len);
  EXPECT_EQ(0, memcmp(via.data(), trace.payload + 9, via.size()));
  EXPECT_EQ(0, memcmp(target.pub_key, trace.payload + 9 + via.size(), width));
  for (int i = 0; i < hops - 1; i++)
    EXPECT_EQ(0, memcmp(via.data() + (hops - 2 - i) * width,
                       trace.payload + 9 + hops * width + i * width, width));
  finish(600); traceReply(1, 1500); tick(5000); finish(5100);
  if (radio.sent.size() == 3 && publicText(2).find("1/2") != std::string::npos) {
    tick(8100); tick(8101); finish(8200);
    EXPECT_NE(std::string::npos, publicText(3).find("Target"));
  } else EXPECT_NE(std::string::npos, publicText(2).find("Target"));
  EXPECT_EQ(32, pool.getFreeCount());
}
INSTANTIATE_TEST_SUITE_P(AllLengthsBothWidths, TargetRoutes,
    ::testing::Combine(::testing::Values(1, 2), ::testing::Range(1, 11)));

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
