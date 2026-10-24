#include "../sensor_mocks/SensorIntegrationFixture.h"

class SensorScope : public SensorIntegration {
protected:
  TransportKey a{}, b{};
  void SetUp() override {
    SensorIntegration::SetUp();
    auto ra = mesh.regions.putRegion("#scope-a", 0);
    ASSERT_NE(nullptr, ra);
    ra->flags = 0;
    ASSERT_EQ(1, mesh.regions.getTransportKeysFor(*ra, &a, 1));
    auto rb = mesh.regions.putRegion("#scope-b", 0);
    ASSERT_NE(nullptr, rb);
    rb->flags = 0;
    ASSERT_EQ(1, mesh.regions.getTransportKeysFor(*rb, &b, 1));
    mesh.default_scope = b;
  }
  std::vector<uint8_t> wire(mesh::Packet packet) {
    std::vector<uint8_t> bytes{packet.header};
    if (packet.hasTransportCodes()) {
      const auto codes = reinterpret_cast<const uint8_t*>(packet.transport_codes);
      bytes.insert(bytes.end(), codes, codes + 4);
    }
    bytes.push_back(packet.path_len);
    bytes.insert(bytes.end(), packet.path, packet.path + packet.getPathByteLen());
    bytes.insert(bytes.end(), packet.payload, packet.payload + packet.payload_len);
    return bytes;
  }
  mesh::Packet parse(const std::vector<uint8_t>& bytes) {
    mesh::Packet packet{};
    EXPECT_TRUE(mesh.tryParsePacket(&packet, bytes.data(), bytes.size()));
    return packet;
  }
  std::vector<uint8_t> scopedCommand(const char* text, const TransportKey& key, uint8_t ch = 0,
                                     uint8_t hops = 2, uint8_t width = 1) {
    auto packet = parse(command(text, hops, &channel.channel(ch), width));
    packet.header = (packet.header & ~PH_ROUTE_MASK) | ROUTE_TYPE_TRANSPORT_FLOOD;
    packet.transport_codes[0] = key.calcTransportCode(&packet);
    packet.transport_codes[1] = 0;
    return wire(packet);
  }
  void expectScope(size_t index, const TransportKey& key, uint8_t ch = 0, uint8_t width = 1) {
    auto packet = parse(radio.sent.at(index));
    EXPECT_EQ(width, packet.getPathHashSize());
    EXPECT_EQ(channel.channel(ch).hash[0], packet.payload[0]);
    if (key.isNull()) EXPECT_EQ(ROUTE_TYPE_FLOOD, packet.getRouteType());
    else {
      EXPECT_EQ(ROUTE_TYPE_TRANSPORT_FLOOD, packet.getRouteType());
      EXPECT_EQ(key.calcTransportCode(&packet), packet.transport_codes[0]);
      EXPECT_EQ(0, packet.transport_codes[1]);
    }
    EXPECT_NE(std::string::npos, publicText(index, &channel.channel(ch)).find("BME680 Sensor:"));
  }
};

TEST_F(SensorScope, ResolvedRequestOverridesDefaultAndCapturesHashWidth) {
  radio.incoming = scopedCommand("Alice: !hillvue ping", a, 1, 2, 2);
  tick(100); tick(3000);
  ASSERT_EQ(1u, radio.sent.size());
  expectScope(0, a, 1, 2);
  EXPECT_EQ("BME680 Sensor: Pong", publicText(0, &channel.channel(1)));
  finish(3100);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorScope, ExplicitUnscopedRequestDoesNotUseDefault) {
  radio.incoming = command("Alice: !hillvue ping");
  tick(100); tick(3000);
  ASSERT_EQ(1u, radio.sent.size());
  expectScope(0, TransportKey{});
}

TEST_F(SensorScope, UnresolvedScopeSnapshotsDefaultBeforeDelayedSend) {
  TransportKey unknown{}; unknown.key[0] = 91;
  radio.incoming = scopedCommand("Alice: !hillvue ping", unknown);
  tick(100);
  mesh.default_scope = a;
  tick(3000);
  ASSERT_EQ(1u, radio.sent.size());
  expectScope(0, b);
}

TEST_F(SensorScope, UnresolvedScopeWithoutDefaultRepliesUnscoped) {
  TransportKey unknown{}; unknown.key[0] = 91;
  mesh.default_scope = {};
  radio.incoming = scopedCommand("Alice: !hillvue ping", unknown);
  tick(100); tick(3000);
  ASSERT_EQ(1u, radio.sent.size());
  expectScope(0, TransportKey{});
}

TEST_F(SensorScope, ResolutionUsesObservedPacketRatherThanPreviousReceive) {
  auto request_a = parse(scopedCommand("Alice: !hillvue trace", a));
  auto request_b = parse(scopedCommand("Bob: !hillvue trace", b));
  auto scope_a = mesh.requestScope(request_a);
  auto scope_b = mesh.requestScope(request_b);
  ASSERT_EQ(mesh::PacketScope::RESOLVED, scope_a.kind);
  ASSERT_EQ(mesh::PacketScope::RESOLVED, scope_b.kind);
  EXPECT_FALSE(scope_a.equals(scope_b));
  EXPECT_TRUE(mesh.requestScope(request_a).equals(scope_a));
  // Renaming an entry does not change identity when the key itself is unchanged.
  auto renamed = scope_a;
  EXPECT_TRUE(renamed.equals(scope_a));
  renamed.key[15] ^= 1;
  EXPECT_FALSE(renamed.equals(scope_a));
}

TEST_F(SensorScope, InterleavedMultipartRepliesKeepScopeChannelAndWidth) {
  for (uint8_t i = 0; i < 20; i++) {
    uint8_t key[PUB_KEY_SIZE]{}; key[0] = 20 + i;
    uint8_t data[MAX_ADVERT_DATA_SIZE];
    auto length = AdvertDataBuilder(ADV_TYPE_REPEATER, "North Ridge Long Name").encodeTo(data);
    bot.onAdvertRecv(nullptr, mesh::Identity(key), 1, data, length);
  }
  radio.incoming = scopedCommand("Alice: !hillvue path", a, 0, 10, 1);
  tick(100); tick(2200); finish(2300);
  radio.incoming = scopedCommand("Bob: !hillvue path", b, 1, 10, 1);
  tick(2400); tick(4600); finish(4700);
  mesh.default_scope = {};
  tick(5300); tick(5301); finish(5400);
  tick(7700); tick(7701); finish(7800);
  ASSERT_EQ(4u, radio.sent.size());
  expectScope(0, a, 0); expectScope(1, b, 1);
  expectScope(2, a, 0); expectScope(3, b, 1);
  EXPECT_NE(std::string::npos, publicText(0).find("1/2"));
  EXPECT_NE(std::string::npos, publicText(2).find("2/2"));
  auto first = parse(radio.sent[0]), second = parse(radio.sent[2]);
  EXPECT_NE(first.transport_codes[0], second.transport_codes[0]);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorScope, TraceResultAndBusyNoticeKeepIndependentContexts) {
  radio.incoming = scopedCommand("Alice: !hillvue trace", a, 0, 2, 2);
  tick(100); finish(200);
  radio.incoming = scopedCommand("Bob: !hillvue trace", b, 1);
  tick(300); tick(3000); finish(3100);
  ASSERT_EQ(2u, radio.sent.size());
  expectScope(1, b, 1);
  EXPECT_NE(std::string::npos, publicText(1, &channel.channel(1)).find("already running"));
  traceReply(0, 3200);
  mesh.default_scope = {};
  tick(7000); finish(7100);
  ASSERT_EQ(3u, radio.sent.size());
  expectScope(2, a, 0, 2);
  EXPECT_NE(std::string::npos, publicText(2).find("3100 ms RTT"));
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorScope, RetryFailureRetainsAcceptedScopeAfterOtherTraffic) {
  radio.incoming = scopedCommand("Alice: !hillvue trace", a);
  tick(100); finish(200);
  radio.incoming = scopedCommand("Bob: !hillvue ping", b, 1);
  tick(300); tick(3000); finish(3100);
  mesh.default_scope = {};
  tick(10100); tick(12200); finish(12300);
  tick(22200); tick(25200); tick(28000); finish(28100);
  ASSERT_EQ(4u, radio.sent.size());
  expectScope(1, b, 1); expectScope(3, a);
  EXPECT_NE(std::string::npos, publicText(3).find("no result after retry"));
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorScope, LateFallbackRetainsOriginalScopeAndDoesNotTransmitAgainAfterClosing) {
  radio.incoming = scopedCommand("Alice: !hillvue trace", a);
  tick(100); finish(200); tick(10100); tick(12200); finish(12300);
  traceReply(0, 13000);
  mesh.default_scope = b;
  tick(22200); tick(26000); finish(26100);
  ASSERT_EQ(3u, radio.sent.size());
  expectScope(2, a);
  traceReply(1, 27000); tick(60000);
  EXPECT_EQ(3u, radio.sent.size());
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorScope, ScopedSuccessSurvivesAllocationFailureWithoutLosingContext) {
  radio.incoming = scopedCommand("Alice: !hillvue trace", a);
  tick(100); finish(200);
  std::vector<mesh::Packet*> held;
  while (pool.getFreeCount() > 1) held.push_back(pool.allocNew());
  traceReply(0, 500);
  mesh.default_scope = b;
  for (auto packet : held) pool.free(packet);
  tick(4000); tick(4001); finish(4100);
  ASSERT_EQ(2u, radio.sent.size());
  expectScope(1, a);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorScope, BusyChannelWaitIsExcludedFromProbeRtt) {
  radio.busy = true;
  radio.incoming = scopedCommand("Alice: !hillvue trace", a);
  tick(100); tick(2000);
  EXPECT_EQ(0u, bot.diagnostics().starts);
  EXPECT_TRUE(radio.sent.empty());
  radio.busy = false;
  tick(4000);
  ASSERT_EQ(1u, bot.diagnostics().starts);
  finish(4100); traceReply(0, 4500);
  EXPECT_EQ(500u, bot.diagnostics().selected.elapsed_millis);
  tick(8000); finish(8100);
  expectScope(1, a);
}

TEST_F(SensorScope, DirectRequestUsesDefaultButDoesNotBorrowRequestScope) {
  auto packet = parse(command("Alice: !hillvue ping"));
  packet.header = (packet.header & ~PH_ROUTE_MASK) | ROUTE_TYPE_DIRECT;
  auto scope = mesh.requestScope(packet);
  auto context = mesh.replyContext(packet, 1, scope);
  EXPECT_EQ(0, memcmp(b.key, context.scope_key, sizeof(b.key)));
  mesh.default_scope = {};
  context = mesh.replyContext(packet, 1, scope);
  TransportKey empty{};
  EXPECT_EQ(0, memcmp(empty.key, context.scope_key, sizeof(empty.key)));
}

TEST_F(SensorScope, RegionRemovalCannotRedirectAnAlreadyAcceptedReply) {
  radio.incoming = scopedCommand("Alice: !hillvue ping", a);
  tick(100);
  mesh.regions.clear();
  mesh.default_scope = b;
  tick(3000);
  ASSERT_EQ(1u, radio.sent.size());
  expectScope(0, a);
}

TEST_F(SensorScope, KeyRotationUnderSameRegionIdDoesNotReuseHistory) {
  auto packet = parse(scopedCommand("Alice: !hillvue trace", a));
  auto old_scope = mesh.requestScope(packet);
  TraceRoute route{};
  ASSERT_EQ(TraceRoute::VALID, TraceRoute::fromRequest(&packet, route));
  TraceRouteHistory history;
  history.record(route, 0, 0, 100, true, 10, old_scope);
  auto region = mesh.regions.findByName("#scope-a");
  ASSERT_NE(nullptr, region);
  auto id = region->id;
  mesh.scope_store.clear();
  TransportKey rotated{};
  mesh.scope_store.getAutoKeyFor(id, "#replacement-key", rotated);
  auto new_packet = parse(scopedCommand("Alice: !hillvue trace", rotated));
  auto new_scope = mesh.requestScope(new_packet);
  ASSERT_EQ(mesh::PacketScope::RESOLVED, new_scope.kind);
  EXPECT_FALSE(old_scope.equals(new_scope));
  EXPECT_EQ(1, history.rank(route, 0, 0, 101, new_scope));
  // Rename with unchanged cached key: scope identity remains the key, not the name.
  strcpy(region->name, "#renamed");
  EXPECT_TRUE(new_scope.equals(mesh.requestScope(new_packet)));
}

TEST_F(SensorScope, RescheduledSecondPartKeepsScopeAfterQueueRejection) {
  auto request = parse(scopedCommand("Alice: !hillvue path", a));
  auto context = mesh.replyContext(request, 1, mesh.requestScope(request));
  std::string text;
  for (int i = 0; i < 10; i++) text += "A delayed scoped response line\n";
  bot.sendPublicResponse(text.c_str(), context);
  tick(2200); finish(2300);
  mesh.default_scope = b;
  mesh.reject_group = 1;
  tick(5300);
  EXPECT_EQ(1u, radio.sent.size());
  tick(6300); tick(6301); finish(6400);
  ASSERT_EQ(2u, radio.sent.size());
  expectScope(0, a, 1); expectScope(1, a, 1);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorScope, MultipartTraceReportKeepsAcceptedScopeAcrossBothParts) {
  for (uint8_t i = 0; i < 10; i++) {
    uint8_t key[PUB_KEY_SIZE]{}; key[0] = 20 + i;
    uint8_t data[MAX_ADVERT_DATA_SIZE];
    auto length = AdvertDataBuilder(ADV_TYPE_REPEATER, "North Ridge Long Name").encodeTo(data);
    bot.onAdvertRecv(nullptr, mesh::Identity(key), 1, data, length);
  }
  radio.incoming = scopedCommand("Alice: !hillvue trace", a, 1, 10);
  tick(100); finish(200); traceReply(0, 500);
  mesh.default_scope = b;
  tick(4000); finish(4100);
  tick(7100); tick(7101); finish(7200);
  ASSERT_EQ(3u, radio.sent.size());
  expectScope(1, a, 1); expectScope(2, a, 1);
  EXPECT_NE(std::string::npos, publicText(1, &channel.channel(1)).find("1/2"));
  EXPECT_NE(std::string::npos, publicText(2, &channel.channel(1)).find("2/2"));
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorScope, ForeignScopeCopyCannotSupplyAnAlternateRetryRoute) {
  auto bytes = scopedCommand("Alice: !hillvue trace", a);
  radio.incoming = bytes;
  tick(100); finish(200);
  auto copy = parse(bytes);
  copy.path[0] = 40; copy.path[1] = 41;
  copy.transport_codes[0] = b.calcTransportCode(&copy);
  radio.incoming = wire(copy);
  tick(300); tick(10100); tick(12200);
  ASSERT_EQ(2u, radio.sent.size());
  auto retry = parse(radio.sent[1]);
  const uint8_t original[] = {21, 20, 21};
  EXPECT_EQ(0, memcmp(original, retry.payload + 9, sizeof(original)));
  finish(12300); traceReply(1, 14000); tick(18000); finish(18100);
  expectScope(2, a);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorScope, SameScopeCopyCanSupplyAFreshAlternateRetryRoute) {
  auto bytes = scopedCommand("Alice: !hillvue trace", a);
  radio.incoming = bytes;
  tick(100); finish(200);
  auto copy = parse(bytes);
  copy.path[0] = 40; copy.path[1] = 41;
  radio.incoming = wire(copy);  // codes authenticate the ciphertext, not the path
  tick(300); tick(10100); tick(12200);
  ASSERT_EQ(2u, radio.sent.size());
  auto retry = parse(radio.sent[1]);
  const uint8_t alternate[] = {41, 40, 41};
  EXPECT_EQ(0, memcmp(alternate, retry.payload + 9, sizeof(alternate)));
  finish(12300); traceReply(1, 14000); tick(18000); finish(18100);
  expectScope(2, a);
  EXPECT_EQ(32, pool.getFreeCount());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
