#include <gtest/gtest.h>
#include <deque>
#include <memory>
#include <tuple>
#include <helpers/RepeaterLogin.h>
#include "../timing_reference/LoRaReferenceVectors.h"
#include "../sensor_mocks/SensorTestRig.h"

// Deterministic packet medium: real MeshCore forwarding, simulated links and airtime.
// Optional overlap/capture/half-duplex model is deliberately approximate, not RF acceptance.
class NetworkRadio : public TestRadio {
public:
  struct Reception { std::vector<uint8_t> bytes; int8_t snr; };
  std::deque<Reception> received;
  const LoRaReferenceProfile* phy=nullptr;
  uint8_t phy_index=0;
  uint32_t timingProfile() const override { return phy?0x10000u+phy_index:airtime; }
  uint32_t getEstAirtimeFor(int length) override {
    if(!phy) return TestRadio::getEstAirtimeFor(length);
    if(transmitting) ++estimates_during_tx;
    return phy->airtime[std::min(255,std::max(0,length))];
  }
  uint32_t started_at = 0;
  bool active = false;
  uint32_t busy_until = 0;
  bool isReceiving() override { return g_mock_millis < busy_until; }
  int recvRaw(uint8_t* bytes, int size) override {
    if (received.empty()) return 0;
    auto packet = received.front();
    received.pop_front();
    int length = std::min(size, (int)packet.bytes.size());
    memcpy(bytes, packet.bytes.data(), length);
    last_snr = packet.snr / 4.0f;
    return length;
  }
  bool startSendRaw(const uint8_t* bytes, int length) override {
    if(phy) airtime=getEstAirtimeFor(length);
    if (!TestRadio::startSendRaw(bytes, length)) return false;
    started_at = g_mock_millis;
    active = true;
    return true;
  }
  bool isSendComplete() override { return active && g_mock_millis - started_at >= airtime; }
  void onSendFinished() override { TestRadio::onSendFinished(); active = false; }
};

class NetworkRelay : public mesh::Mesh {
public:
  bool forwards;
  bool scoped_only = false;
  TransportKey required_scope{};
  unsigned scope_drops = 0;
  ClientACL discovery_clients;
  int discovery_matches[24]{};
  unsigned discovery_requests = 0;
  bool discovery_enabled = false;
  bool public_guest = true;
  bool accept_foreign_local_control = false;
  TransportKey fallback_scope{};
  unsigned login_requests = 0;
  PublicChannelSensorBot channels;
  std::vector<std::string> messages, acknowledgements;
  uint32_t direct_delay = 0, flood_delay = 0;
  bool standard_delay = false;
  NetworkRelay(NetworkRadio& radio, TestClock& clock, TestRandom& random, TestRTC& rtc,
               StaticPoolPacketManager& pool, SimpleMeshTables& tables, bool forward)
      : Mesh(radio, clock, random, rtc, pool, tables), forwards(forward) { }
protected:
  mesh::DispatcherAction onRecvPacket(mesh::Packet* packet) override {
    if (scoped_only && packet->isRouteFlood() &&
        (packet->getRouteType() != ROUTE_TYPE_TRANSPORT_FLOOD ||
         packet->transport_codes[0] != required_scope.calcTransportCode(packet))) {
      bool local=accept_foreign_local_control && packet->payload_len>0 && self_id.isHashMatch(packet->payload) &&
          (packet->getPayloadType()==PAYLOAD_TYPE_REQ || packet->getPayloadType()==PAYLOAD_TYPE_ANON_REQ);
      if(!local) { scope_drops++; return ACTION_RELEASE; }
    }
    return Mesh::onRecvPacket(packet);
  }
  int searchPeersByHash(const uint8_t* hash) override {
    int count = 0;
    if (discovery_enabled) for (int i = 0; i < discovery_clients.getNumClients() && count < 24; i++)
      if (discovery_clients.getClientByIdx(i)->id.isHashMatch(hash)) discovery_matches[count++] = i;
    return count;
  }
  void getPeerSharedSecret(uint8_t* secret, int index) override {
    memcpy(secret, discovery_clients.getClientByIdx(discovery_matches[index])->shared_secret, 32);
  }
  void onPeerDataRecv(mesh::Packet* packet, uint8_t type, int index, const uint8_t* secret,
                       uint8_t* data, size_t length) override {
    if (!discovery_enabled || type != PAYLOAD_TYPE_REQ || length < 9 || data[4] != 3) return;
    auto client = discovery_clients.getClientByIdx(discovery_matches[index]);
    uint32_t tag; memcpy(&tag, data, 4);
    if (!isFreshRepeaterRequest(*client,tag)) return;  // real repeater replay protection
    commitRepeaterRequest(*client,tag,rtcTime()); discovery_requests++;
    uint8_t extra[5]{}; memcpy(extra, &tag, 4); extra[4] = 0xaa;
    auto reply = createPathReturn(client->id, secret, packet->path, packet->path_len,
                                  PAYLOAD_TYPE_RESPONSE, extra, sizeof(extra));
    if (reply) mesh::sendCapturedFlood(static_cast<mesh::Mesh&>(*this),
              replyScope(*packet), reply, 100, packet->getPathHashSize());
  }
  void onAnonDataRecv(mesh::Packet* packet,const uint8_t* secret,const mesh::Identity& sender,
                      uint8_t* data,size_t length) override {
    if(!discovery_enabled || length<4) return;
    data[length]=0; uint32_t tag; memcpy(&tag,data,4); bool persist=false;
    auto client=authenticateRepeaterClient(discovery_clients,sender,secret,tag,rtcTime(),
        (const char*)data+4,"admin-secret",public_guest?"":"protected",persist);
    ++login_requests;
    if(!client) return;
    uint8_t body[13],nonce[4]{};
    encodeRepeaterLoginReply(body,*client,getRTCClock()->getCurrentTimeUnique(),nonce,2);
    auto reply=createPathReturn(sender,secret,packet->path,packet->path_len,PAYLOAD_TYPE_RESPONSE,body,sizeof(body));
    if(reply) mesh::sendCapturedFlood(static_cast<mesh::Mesh&>(*this),replyScope(*packet),reply,100,packet->getPathHashSize());
  }
  TransportKey replyScope(const mesh::Packet& packet) {
    bool known=packet.hasTransportCodes() && required_scope.calcTransportCode(&packet)==packet.transport_codes[0];
    auto selected=mesh::chooseReplyScope(known,packet.getRouteType()==ROUTE_TYPE_FLOOD,!fallback_scope.isNull());
    return selected==mesh::REPLY_SCOPE_REQUEST?required_scope:selected==mesh::REPLY_SCOPE_DEFAULT?fallback_scope:TransportKey{};
  }
  uint32_t rtcTime() { return getRTCClock()->getCurrentTime(); }
  int searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel dest[], int capacity) override {
    return channels.findChannel(hash, dest, capacity);
  }
  void onGroupDataRecv(mesh::Packet*, uint8_t, const mesh::GroupChannel&,
                       uint8_t* data, size_t length) override {
    if (length > 5) {
      std::string message((char*)data + 5, strnlen((char*)data + 5, length - 5));
      if (message.find("Trace requested") != std::string::npos)
        acknowledgements.push_back(message);
      else messages.push_back(message);
    }
  }
  bool allowPacketForward(const mesh::Packet*) override { return forwards; }
  int calcRxDelay(float, uint32_t) const override { return 0; }
  uint32_t getRetransmitDelay(const mesh::Packet*) override { return flood_delay; }
  uint32_t getDirectRetransmitDelay(const mesh::Packet* packet) override {
    if (!standard_delay) return direct_delay;
    uint32_t delay = _radio->getEstAirtimeFor(packet->getRawLength()) * 3 / 10;
    return getRNG()->nextInt(0, 5 * delay + 1);
  }
};

class SensorNetwork : public ::testing::Test {
protected:
  static const int COUNT = 6, SENSOR = 4;
  TestClock clock;
  TestRTC rtc;
  TestRandom random;
  NetworkRadio radios[COUNT];
  SimpleMeshTables tables[COUNT];
  std::unique_ptr<StaticPoolPacketManager> pools[COUNT];
  std::unique_ptr<NetworkRelay> relays[COUNT];
  std::unique_ptr<TestMesh> sensor_mesh;
  mesh::Mesh* meshes[COUNT]{};
  std::unique_ptr<SensorBotService> bot;
  Bme680Telemetry snapshot;
  MemoryFileSystem fs;
  PublicChannelSensorBot channel;
  bool links[COUNT][COUNT]{};
  size_t consumed[COUNT]{};
  int drop_sensor_traces = 0;
  bool rf_model = false, capture = false;
  uint32_t collision_drops = 0, half_duplex_drops = 0, received_signals = 0;
  uint32_t tx_until[COUNT]{}, loss_seed = 1;
  uint8_t loss_percent = 0;
  int power[COUNT][COUNT]{};
  int8_t link_snrs[COUNT][COUNT]{};
  struct Delivery { uint32_t due; int target; std::vector<uint8_t> bytes;
                    uint32_t start; int power; bool lost; int8_t snr; };
  std::vector<Delivery> pending;

  void emit(int source, const std::vector<uint8_t>& bytes, uint32_t now, uint32_t duration) {
    uint32_t end = now + duration;
    tx_until[source] = end;
    if (rf_model) for (auto& incoming : pending)
      if (incoming.target == source && incoming.due > now && !incoming.lost) {
        incoming.lost = true; half_duplex_drops++;
      }
    for (int target = 0; target < COUNT; target++) {
      if (!links[source][target]) continue;
      Delivery delivery{end, target, bytes, now, power[source][target], false, link_snrs[source][target]};
      if (rf_model) {
        radios[target].busy_until = std::max(radios[target].busy_until, end);
        if (tx_until[target] > now) { delivery.lost = true; half_duplex_drops++; }
        for (auto& other : pending) {
          if (other.target != target || other.due <= now) continue;
          if (!capture || abs(delivery.power - other.power) < 6) {
            if (!other.lost) collision_drops++;
            other.lost = true;
            if (!delivery.lost) collision_drops++;
            delivery.lost = true;
          } else if (delivery.power > other.power) {
            if (!other.lost) collision_drops++;
            other.lost = true;
          } else {
            if (!delivery.lost) collision_drops++;
            delivery.lost = true;
          }
        }
      }
      pending.push_back(delivery);
    }
  }

  void connect(int a, int b) { links[a][b] = links[b][a] = true; }
  void SetUp() override {
    for (int i = 0; i < COUNT; i++) {
      pools[i].reset(new StaticPoolPacketManager(32));
      if (i == SENSOR) {
        auto sensor = new TestMesh(radios[i], clock, random, rtc, *pools[i], tables[i]);
        bot.reset(new SensorBotService(*sensor, snapshot));
        sensor->bot = bot.get();
        sensor_mesh.reset(sensor);
        meshes[i] = sensor;
      } else {
        relays[i].reset(new NetworkRelay(radios[i], clock, random, rtc, *pools[i], tables[i], i != 0));
        meshes[i] = relays[i].get();
      }
      memset(meshes[i]->self_id.pub_key, 0, PUB_KEY_SIZE);
      meshes[i]->self_id.pub_key[0] = 19 + i;
      meshes[i]->self_id.pub_key[1] = 100 + i;
      meshes[i]->begin();
    }
    bot->begin(&fs);
    connect(0, 1); connect(1, 2); connect(2, 3); connect(3, SENSOR);
  }
  NetworkRelay& relay(int index) { return *relays[index]; }
  void runUntil(uint32_t end) {
    for (uint32_t now = g_mock_millis + 10; now <= end; now += 10) {
      g_mock_millis = now;
      for (size_t i = 0; i < pending.size();) {
        if (pending[i].due > now) { i++; continue; }
        loss_seed = loss_seed * 1664525UL + 1013904223UL;
        if (!pending[i].lost && (loss_seed >> 16) % 100 >= loss_percent) {
          radios[pending[i].target].received.push_back({pending[i].bytes, pending[i].snr});
          received_signals++;
        }
        pending.erase(pending.begin() + i);
      }
      bot->beforeRadioLoop();
      for (auto& mesh : meshes) mesh->loop();
      bot->loop();
      for (int source = 0; source < COUNT; source++) {
        while (consumed[source] < radios[source].sent.size()) {
          const auto bytes = radios[source].sent[consumed[source]++];
          uint8_t type = (bytes[0] >> PH_TYPE_SHIFT) & PH_TYPE_MASK;
          if (source == SENSOR && type == PAYLOAD_TYPE_TRACE && drop_sensor_traces > 0) {
            drop_sensor_traces--;
            continue;
          }
          emit(source, bytes, now, radios[source].airtime);
        }
      }
    }
  }
  void request(uint8_t width = 1, uint8_t channel_index = 0, const TransportKey* scope = nullptr) {
    uint8_t payload[64]{};
    uint32_t time = 1800000000 + g_mock_millis / 1000;
    memcpy(payload, &time, 4);
    const char* text = "Alice: !hillvue trace";
    strcpy((char*)payload + 5, text);
    auto packet = meshes[0]->createGroupDatagram(PAYLOAD_TYPE_GRP_TXT, channel.channel(channel_index),
                                                payload, 5 + strlen(text));
    ASSERT_NE(nullptr, packet);
    mesh::sendCapturedFlood(*meshes[0], scope ? *scope : TransportKey{}, packet, 0, width);
  }
  int transmitted(int node, uint8_t type) {
    int count = 0;
    for (const auto& bytes : radios[node].sent)
      if (((bytes[0] >> PH_TYPE_SHIFT) & PH_TYPE_MASK) == type) count++;
    return count;
  }
  std::vector<std::string> reports(uint8_t channel_index = 0) {
    std::vector<std::string> result;
    for (const auto& bytes : radios[SENSOR].sent) {
      if (((bytes[0] >> PH_TYPE_SHIFT) & PH_TYPE_MASK) != PAYLOAD_TYPE_GRP_TXT) continue;
      uint8_t data[MAX_PACKET_PAYLOAD]{};
      mesh::Packet packet{};
      EXPECT_TRUE(sensor_mesh->tryParsePacket(&packet, bytes.data(), bytes.size()));
      EXPECT_EQ(channel.channel(channel_index).hash[0], packet.payload[0]);
      int length = mesh::Utils::MACThenDecrypt(channel.channel(channel_index).secret, data,
                                              packet.payload + 1, packet.payload_len - 1);
      EXPECT_GT(length, 5);
      std::string message((char*)data + 5, strnlen((char*)data + 5, length > 5 ? length - 5 : 0));
      // reports() describes final outcomes; acknowledgements still traverse the
      // real medium and are asserted separately below.
      if (message.find("Trace requested") == std::string::npos)
        result.push_back(message);
    }
    return result;
  }
  void expectCleanPools() {
    for (auto& pool : pools) EXPECT_EQ(32, pool->getFreeCount());
    EXPECT_FALSE(fs.exists("/s_contacts"));
  }
};

TEST_F(SensorNetwork, ThreeRepeatersActuallyForwardTheOutwardAndReturnLegs) {
  request();
  runUntil(10000);
  auto messages = reports();
  ASSERT_EQ(1u, messages.size());
  EXPECT_NE(std::string::npos, messages[0].find(" ms RTT"));
  EXPECT_NE(std::string::npos, messages[0].find("3 14 +0.0 dB"));
  EXPECT_EQ(1, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  EXPECT_EQ(1, transmitted(1, PAYLOAD_TYPE_TRACE));
  EXPECT_EQ(2, transmitted(2, PAYLOAD_TYPE_TRACE));
  EXPECT_EQ(2, transmitted(3, PAYLOAD_TYPE_TRACE));
  expectCleanPools();
}

TEST_F(SensorNetwork, TwoRepeatersAndTwoByteHashesUseThePublicChannel) {
  links[2][3] = links[3][2] = false;
  links[3][SENSOR] = links[SENSOR][3] = false;
  connect(2, SENSOR);
  request(2, 1);
  runUntil(10000);
  auto messages = reports(1);
  ASSERT_EQ(1u, messages.size());
  EXPECT_NE(std::string::npos, messages[0].find(" ms RTT"));
  EXPECT_EQ(1, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  expectCleanPools();
}

TEST_F(SensorNetwork, AsymmetricLinkAveragesFollowActualForwardingOnBothHashWidths) {
  link_snrs[SENSOR][3] = -24; link_snrs[3][SENSOR] = -8;
  link_snrs[3][2] = 8; link_snrs[2][3] = 0;
  link_snrs[2][1] = -40; link_snrs[1][2] = 8;
  // The requester link is not traversed by the trace and must not enter the report.
  link_snrs[0][1] = -128; link_snrs[1][0] = 127;
  for (uint8_t width : {1, 2}) {
    request(width);
    runUntil(width * 15000);
    auto messages = reports();
    ASSERT_EQ(width, messages.size());
    EXPECT_NE(std::string::npos, messages.back().find(" ms RTT (avg SNR)"));
    if (width == 1) {
      EXPECT_NE(std::string::npos, messages.back().find("1 16 -4.0 dB\n2 15 +1.0 dB\n3 14 -4.0 dB"));
    } else {
      EXPECT_NE(std::string::npos, messages.back().find("1 1667 -4.0 dB\n2 1566 +1.0 dB\n3 1465 -4.0 dB"));
    }
    const uint8_t expected[] = {uint8_t(-24), 8, uint8_t(-40), 8, 0, uint8_t(-8)};
    EXPECT_EQ(0, memcmp(expected, bot->diagnostics().selected.snrs, sizeof(expected)));
    expectCleanPools();
  }
}

TEST_F(SensorNetwork, OneDroppedProbeIsRecoveredByOneSilentRetry) {
  drop_sensor_traces = 1;
  request();
  runUntil(35000);
  auto messages = reports();
  ASSERT_EQ(1u, messages.size());
  EXPECT_NE(std::string::npos, messages[0].find(" ms RTT"));
  EXPECT_EQ(2, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  expectCleanPools();
}

TEST_F(SensorNetwork, RetryUsesAlternateFloodPathWhenTheOriginalReverseLinkFails) {
  links[SENSOR][3] = false;
  connect(2, 5); connect(5, SENSOR);
  relay(5).flood_delay = 1000;
  request();
  runUntil(35000);
  auto messages = reports();
  ASSERT_EQ(1u, messages.size());
  EXPECT_NE(std::string::npos, messages[0].find("1 18 +0.0 dB"));
  EXPECT_EQ(2, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  expectCleanPools();
}

TEST_F(SensorNetwork, WorkingFloodMessagingDoesNotImplyTheTraceLoopCanComplete) {
  links[3][SENSOR] = false;
  links[SENSOR][2] = false;
  links[2][SENSOR] = true;
  request();
  runUntil(40000);
  auto messages = reports();
  ASSERT_EQ(1u, messages.size());
  EXPECT_EQ("BME680 Sensor: Trace: no result after retry", messages[0]);
  EXPECT_EQ(2, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  ASSERT_EQ(1u, relay(0).messages.size());
  EXPECT_EQ(messages[0], relay(0).messages[0]);
  expectCleanPools();
}

TEST_F(SensorNetwork, SlowAirtimeUsesTheCalculatedBudgetInsteadOfRetryingTooSoon) {
  for (auto& radio : radios) radio.airtime = 1000;
  for (int i : { 1, 2, 3 }) relay(i).direct_delay = 2500;
  request();
  runUntil(60000);
  auto messages = reports();
  ASSERT_EQ(1u, messages.size());
  EXPECT_NE(std::string::npos, messages[0].find(" ms RTT"));
  EXPECT_EQ(1, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  expectCleanPools();
}

TEST_F(SensorNetwork, DelayedFirstLoopRemainsUsefulAfterTheRetryHasStarted) {
  for (int i : { 1, 2, 3 }) relay(i).direct_delay = 3000;
  request();
  runUntil(60000);
  auto messages = reports();
  ASSERT_EQ(1u, messages.size());
  EXPECT_NE(std::string::npos, messages[0].find(" ms RTT"));
  EXPECT_EQ(2, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  expectCleanPools();
}

TEST_F(SensorNetwork, HiddenTerminalsCollideAndTheCaptureModelSelectsTheStrongerSignal) {
  memset(links, 0, sizeof(links));
  connect(1, 3); connect(5, 3);
  rf_model = true;
  emit(1, { 0 }, 0, 100); emit(5, { 0 }, 0, 100);
  runUntil(200);
  EXPECT_EQ(0u, received_signals);
  EXPECT_GE(collision_drops, 2u);
  capture = true;
  power[1][3] = -60; power[5][3] = -80;
  emit(1, { 0 }, 200, 100); emit(5, { 0 }, 200, 100);
  runUntil(400);
  EXPECT_EQ(1u, received_signals);
  expectCleanPools();
}

TEST_F(SensorNetwork, HalfDuplexTransmittingNodeCannotReceiveAnOverlappingFrame) {
  memset(links, 0, sizeof(links));
  connect(1, 3);
  rf_model = true;
  emit(1, { 0 }, 0, 100); emit(3, { 0 }, 0, 100);
  runUntil(200);
  EXPECT_EQ(0u, received_signals);
  EXPECT_GE(half_duplex_drops, 2u);
  expectCleanPools();
}

TEST_F(SensorNetwork, BusyChannelDefersInitialProbeWithoutConsumingItsRttBudget) {
  rf_model = true;
  request();
  runUntil(500);
  emit(3, { 0 }, 500, 2000);
  runUntil(2000);
  EXPECT_EQ(0, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  runUntil(15000);
  EXPECT_EQ(1, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  ASSERT_TRUE(bot->diagnostics().has_result);
  EXPECT_LT(bot->diagnostics().selected.elapsed_millis, 10000u);
  EXPECT_EQ(TraceReportDelivery::DELIVERED, bot->diagnostics().delivery);
  expectCleanPools();
}

TEST_F(SensorNetwork, CorrelatedLossAcrossBothAttemptsStillProducesOneBoundedFailure) {
  rf_model = true;
  drop_sensor_traces = 2;
  request();
  runUntil(60000);
  ASSERT_EQ(1u, reports().size());
  EXPECT_EQ("BME680 Sensor: Trace: no result after retry", reports()[0]);
  EXPECT_EQ(2, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  EXPECT_LE(bot->diagnostics().probe_submissions, 2u);
  expectCleanPools();
}

class SensorHashCollision : public SensorNetwork, public ::testing::WithParamInterface<int> { };
TEST_P(SensorHashCollision, HashWidthControlsWhetherCompetingRepeatersForwardTheProbe) {
  rf_model = true;
  meshes[5]->self_id.pub_key[0] = meshes[2]->self_id.pub_key[0];
  connect(1, 5); connect(3, 5);
  relay(5).flood_delay = 300;
  request(GetParam());
  runUntil(60000);
  auto messages = reports();
  ASSERT_EQ(1u, messages.size());
  if (GetParam() == 1) {
    EXPECT_GT(collision_drops, 0u);
    EXPECT_FALSE(bot->diagnostics().has_result);
    EXPECT_EQ(2, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  } else {
    EXPECT_TRUE(bot->diagnostics().has_result);
    // The acknowledgement flood also contends for airtime; a valid two-byte
    // route may need the second attempt without a short-hash misidentification.
    EXPECT_LE(transmitted(SENSOR, PAYLOAD_TYPE_TRACE), 2);
    EXPECT_EQ(1, bot->diagnostics().selected.route.flags);
  }
  expectCleanPools();
}
INSTANTIATE_TEST_SUITE_P(OneAndTwoBytes, SensorHashCollision, ::testing::Values(1, 2));

class SensorContention : public SensorNetwork, public ::testing::WithParamInterface<int> { };
TEST_P(SensorContention, SeededLossAndStandardRepeaterDelaysRemainBounded) {
  rf_model = true;
  loss_percent = 5;
  loss_seed = GetParam();
  random.seed(GetParam());
  for (int i : { 1, 2, 3 }) relay(i).standard_delay = true;
  request();
  runUntil(90000);
  EXPECT_LE(transmitted(SENSOR, PAYLOAD_TYPE_TRACE), 2);
  EXPECT_LE(reports().size(), 1u);
  EXPECT_LE(bot->diagnostics().starts, 2u);
  expectCleanPools();
}
INSTANTIATE_TEST_SUITE_P(DeterministicSeeds, SensorContention, ::testing::Range(1, 9));

class ScopedSensorNetwork : public SensorNetwork {
protected:
  TransportKey scope{};
  void SetUp() override {
    SensorNetwork::SetUp();
    auto region = sensor_mesh->regions.putRegion("#test-region", 0);
    ASSERT_NE(nullptr, region);
    region->flags = 0;
    ASSERT_EQ(1, sensor_mesh->regions.getTransportKeysFor(*region, &scope, 1));
    for (auto& relay : relays) if (relay) {
      relay->scoped_only = true;
      relay->required_scope = scope;
    }
  }
  void expectReportCodes() {
    for (const auto& bytes : radios[SENSOR].sent) {
      mesh::Packet packet{};
      ASSERT_TRUE(sensor_mesh->tryParsePacket(&packet, bytes.data(), bytes.size()));
      if (packet.getPayloadType() != PAYLOAD_TYPE_GRP_TXT) continue;
      EXPECT_EQ(ROUTE_TYPE_TRANSPORT_FLOOD, packet.getRouteType());
      EXPECT_EQ(scope.calcTransportCode(&packet), packet.transport_codes[0]);
    }
  }
};

TEST_F(ScopedSensorNetwork, ScopedReportsCrossRelaysRejectingUnscopedFloodsOnBothChannels) {
  for (uint8_t ch = 0; ch < 2; ch++) {
    request(ch + 1, ch, &scope);
    runUntil(10000 + ch * 15000);
    ASSERT_EQ(ch + 1u, relay(0).messages.size());
    ASSERT_EQ(ch + 1u, relay(0).acknowledgements.size());
    EXPECT_NE(std::string::npos, relay(0).messages.back().find(" ms RTT"));
  }
  expectReportCodes();
  expectCleanPools();
}

TEST_F(ScopedSensorNetwork, WrongScopeAndUnscopedCommandsAreRejectedByTheSameRelayFilter) {
  request();
  runUntil(3000);
  EXPECT_EQ(0, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  TransportKey wrong{}; wrong.key[0] = 73;
  request(1, 0, &wrong);
  runUntil(6000);
  EXPECT_EQ(0, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  EXPECT_GE(relay(1).scope_drops, 2u);
  EXPECT_TRUE(relay(0).messages.empty());
  // Exercise the same negative controls in the report direction as well.
  SensorReplyContext wrong_context(1);
  memcpy(wrong_context.scope_key, wrong.key, sizeof(wrong.key));
  bot->sendPublicResponse("Wrong scoped report", wrong_context);
  bot->sendPublicResponse("Unscoped report", SensorReplyContext{0});
  runUntil(11000);
  EXPECT_EQ(2, transmitted(SENSOR, PAYLOAD_TYPE_GRP_TXT));
  EXPECT_GE(relay(3).scope_drops, 2u);
  EXPECT_TRUE(relay(0).messages.empty());
  expectCleanPools();
}

TEST_F(ScopedSensorNetwork, RetryFailureReportCrossesScopedRelays) {
  drop_sensor_traces = 2;
  request(1, 1, &scope);
  runUntil(60000);
  ASSERT_EQ(1u, relay(0).messages.size());
  EXPECT_NE(std::string::npos, relay(0).messages[0].find("no result after retry"));
  EXPECT_EQ(2, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  expectReportCodes();
  expectCleanPools();
}

TEST_F(ScopedSensorNetwork, DelayedMultipartRepliesCrossScopedRelaysAfterDefaultChanges) {
  mesh::Packet request{};
  request.header = ROUTE_TYPE_TRANSPORT_FLOOD | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
  request.payload_len = 10;
  memset(request.payload, 42, request.payload_len);
  request.setPathHashSizeAndCount(2, 2);
  request.transport_codes[0] = scope.calcTransportCode(&request);
  const auto context = sensor_mesh->replyContext(request, 1, sensor_mesh->requestScope(request));
  std::string text;
  for (int i = 0; i < 10; i++) text += "Scoped multipart report line\n";
  bot->sendPublicResponse(text.c_str(), context);
  sensor_mesh->default_scope.key[0] = 99;
  runUntil(12000);
  ASSERT_EQ(2u, relay(0).messages.size());
  EXPECT_NE(std::string::npos, relay(0).messages[0].find("1/2"));
  EXPECT_NE(std::string::npos, relay(0).messages[1].find("2/2"));
  expectReportCodes();
  expectCleanPools();
}

class TargetedSensorNetwork : public ScopedSensorNetwork {
protected:
  void SetUp() override {
    ScopedSensorNetwork::SetUp();
    for (int node = 0; node < COUNT; node++) {
      bool unique;
      do {
        meshes[node]->self_id = mesh::LocalIdentity(&random); unique = true;
        for (int earlier = 0; earlier < node; earlier++)
          if (meshes[earlier]->self_id.pub_key[0] == meshes[node]->self_id.pub_key[0]) unique = false;
      } while (!unique);
    }
    bot->begin(&fs); // Bind durable files to the final simulated sensor identity.
    relay(1).discovery_enabled = true;
    auto client = relay(1).discovery_clients.putClient(sensor_mesh->self_id, PERM_ACL_GUEST);
    relay(1).self_id.calcSharedSecret(client->shared_secret, sensor_mesh->self_id);
    uint8_t app[MAX_ADVERT_DATA_SIZE];
    auto length = AdvertDataBuilder(ADV_TYPE_REPEATER, "Target").encodeTo(app);
    bot->onAdvertRecv(nullptr, relay(1).self_id, 1, app, length);
  }
  void targetedRequest(uint8_t width, uint8_t ch) {
    uint8_t payload[80]{};
    uint32_t time = 1800000000 + g_mock_millis / 1000;
    memcpy(payload, &time, 4);
    int length = snprintf((char*)payload + 5, sizeof(payload) - 5, "Alice: !hillvue trace %02x%02x",
                relay(1).self_id.pub_key[0], relay(1).self_id.pub_key[1]);
    auto packet = meshes[0]->createGroupDatagram(PAYLOAD_TYPE_GRP_TXT, channel.channel(ch), payload, 5 + length);
    ASSERT_NE(nullptr, packet);
    mesh::sendCapturedFlood(*meshes[0], scope, packet, 0, width);
  }
};

class TargetedNetworkRoutes : public TargetedSensorNetwork,
                             public ::testing::WithParamInterface<std::tuple<int, int>> {};
TEST_P(TargetedNetworkRoutes, AuthenticatedScopedDiscoveryAndLoopReachSelectedRepeater) {
  auto width = std::get<0>(GetParam()), ch = std::get<1>(GetParam());
  targetedRequest(width, ch); runUntil(15000);
  ASSERT_EQ(1u, relay(0).acknowledgements.size());
  EXPECT_EQ("BME680 Sensor: Trace requested to Target, please wait up to " + std::to_string(bot->timing().seconds()) + " seconds.", relay(0).acknowledgements[0]);
  ASSERT_EQ(1u, relay(0).messages.size());
  EXPECT_NE(std::string::npos, relay(0).messages[0].find("avg SNR"));
  EXPECT_NE(std::string::npos, relay(0).messages[0].find("3 Target"));
  EXPECT_EQ(1u, relay(1).discovery_requests);
  EXPECT_EQ(1, transmitted(SENSOR, PAYLOAD_TYPE_REQ));
  EXPECT_EQ(1, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  EXPECT_EQ(0, sensor_mesh->management_acl.getNumClients());
  EXPECT_EQ(PERM_ACL_GUEST, relay(1).discovery_clients.getClientByIdx(0)->permissions);
  expectReportCodes(); expectCleanPools();
}
INSTANTIATE_TEST_SUITE_P(BothChannelsAndWidths, TargetedNetworkRoutes,
    ::testing::Combine(::testing::Values(1, 2), ::testing::Values(0, 1)));

TEST_F(TargetedSensorNetwork, FreshUnknownClientUsesLoginPathWithoutRedundantDiscovery) {
  relay(1).discovery_clients.load(&fs,relay(1).self_id);
  ASSERT_EQ(0,relay(1).discovery_clients.getNumClients());
  targetedRequest(1,0); runUntil(60000);
  ASSERT_EQ(1u,relay(0).messages.size());
  EXPECT_NE(std::string::npos,relay(0).messages[0].find("avg SNR"));
  EXPECT_EQ(1u,relay(1).login_requests); EXPECT_EQ(0u,relay(1).discovery_requests);
  EXPECT_EQ(1,transmitted(SENSOR,PAYLOAD_TYPE_REQ)); EXPECT_EQ(1,transmitted(SENSOR,PAYLOAD_TYPE_ANON_REQ));
  EXPECT_EQ(1,transmitted(SENSOR,PAYLOAD_TYPE_TRACE)); expectCleanPools();
  const auto& evidence=bot->diagnostics();
  EXPECT_TRUE(evidence.control[0].sent);EXPECT_TRUE(evidence.control[1].sent);EXPECT_FALSE(evidence.control[2].sent);
  EXPECT_TRUE(evidence.login_path);
  EXPECT_GT(evidence.control[1].tag,evidence.control[0].tag); EXPECT_EQ(0u,evidence.control[2].tag);
  EXPECT_GE(evidence.control[1].completed,evidence.control[1].started);
  EXPECT_LE(radios[SENSOR].sent.size(),8u);
  // A stock repeater reboot loses transient guest clients, not its identity.
  relay(1).discovery_clients.load(&fs,relay(1).self_id);
  ASSERT_EQ(0,relay(1).discovery_clients.getNumClients());
  targetedRequest(1,0);runUntil(120000);
  ASSERT_EQ(2u,relay(0).messages.size());EXPECT_NE(std::string::npos,relay(0).messages.back().find("avg SNR"));
  EXPECT_EQ(2u,relay(1).login_requests);EXPECT_EQ(2,transmitted(SENSOR,PAYLOAD_TYPE_TRACE));expectCleanPools();
}

TEST_F(TargetedSensorNetwork, ReplayProtectionUsesMonotonicRtcTagsRatherThanTraceRandomIds) {
  relay(1).discovery_clients.getClientByIdx(0)->last_timestamp = 1799999999;
  targetedRequest(2, 1); runUntil(15000);
  ASSERT_EQ(1u, relay(0).messages.size());
  EXPECT_GE(relay(1).discovery_clients.getClientByIdx(0)->last_timestamp, 1800000000u);
  EXPECT_EQ(1u, relay(1).discovery_requests);
  EXPECT_EQ(1, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
  expectCleanPools();
}

TEST_F(TargetedSensorNetwork, VerifiedAdvertFallbackPersistsAndWorksWithoutRemoteLogin) {
  relay(1).discovery_enabled=false;
  uint8_t app[40]; auto length=AdvertDataBuilder(ADV_TYPE_REPEATER,"Target").encodeTo(app);
  auto advertisement=meshes[1]->createAdvert(meshes[1]->self_id,app,length);
  ASSERT_NE(nullptr,advertisement);
  mesh::sendCapturedFlood(*meshes[1],scope,advertisement,0,2);
  runUntil(3000);
  EXPECT_GT(bot->diagnostics().advert_observations,0u);
  targetedRequest(2,1);runUntil(60000);
  ASSERT_EQ(1u,relay(0).messages.size());
  EXPECT_NE(std::string::npos,relay(0).messages[0].find("avg SNR"));
  EXPECT_TRUE(bot->diagnostics().advert_route);
  EXPECT_EQ(1,transmitted(SENSOR,PAYLOAD_TYPE_TRACE)); EXPECT_TRUE(fs.exists("/trace_adverts"));
  expectCleanPools();
  // Recreate the service with retained identity/files but no fresh advert.
  bot.reset(new SensorBotService(*sensor_mesh,snapshot)); sensor_mesh->bot=bot.get(); bot->begin(&fs);
  targetedRequest(2,1);runUntil(120000);
  ASSERT_EQ(2u,relay(0).messages.size());
  EXPECT_NE(std::string::npos,relay(0).messages.back().find("avg SNR")) << relay(0).messages.back();
  EXPECT_TRUE(bot->diagnostics().advert_route); EXPECT_TRUE(bot->diagnostics().advert_probation);
  EXPECT_EQ(2,transmitted(SENSOR,PAYLOAD_TYPE_TRACE)); expectCleanPools();
}

TEST_F(TargetedSensorNetwork, ProtectedGuestCannotBlockAdvertFallback) {
  relay(1).discovery_clients.load(&fs,relay(1).self_id); relay(1).public_guest=false;
  uint8_t app[40];auto len=AdvertDataBuilder(ADV_TYPE_REPEATER,"Target").encodeTo(app);
  auto advertisement=meshes[1]->createAdvert(meshes[1]->self_id,app,len);
  mesh::sendCapturedFlood(*meshes[1],scope,advertisement,0,1);runUntil(3000);
  targetedRequest(1,0);runUntil(60000);
  ASSERT_EQ(1u,relay(0).messages.size());EXPECT_NE(std::string::npos,relay(0).messages[0].find("avg SNR"));
  EXPECT_EQ(3u,relay(1).login_requests); EXPECT_EQ(0,relay(1).discovery_clients.getNumClients());
  EXPECT_TRUE(bot->diagnostics().advert_route);expectCleanPools();
}

TEST_F(TargetedSensorNetwork, StockReplyScopeFallbackIsAcceptedWithFrozenPublicContext) {
  connect(1,SENSOR);
  relay(1).accept_foreign_local_control=true; relay(1).fallback_scope=scope;
  sensor_mesh->default_scope.key[0]=0x45; // Target/intermediates do not know this outgoing scope.
  targetedRequest(1,0);runUntil(15000);
  ASSERT_EQ(1u,relay(0).messages.size());EXPECT_NE(std::string::npos,relay(0).messages[0].find("avg SNR"));
  EXPECT_TRUE(bot->diagnostics().has_result); EXPECT_GT(bot->diagnostics().scope_mismatches,0u);
  expectReportCodes();expectCleanPools();
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST_F(TargetedSensorNetwork, AustraliaNarrowUsesVariablePacketAirtimeAndLoginReturnPath) {
  for(auto& radio:radios) { radio.phy=&LORA_REFERENCE[0]; radio.phy_index=0; }
  relay(1).discovery_clients.load(&fs,relay(1).self_id);
  targetedRequest(1,0); runUntil(120000);
  ASSERT_EQ(1u,relay(0).messages.size()); EXPECT_NE(std::string::npos,relay(0).messages[0].find("avg SNR"));
  EXPECT_EQ(1u,relay(1).login_requests); EXPECT_EQ(0u,relay(1).discovery_requests);
  EXPECT_EQ(1,transmitted(SENSOR,PAYLOAD_TYPE_REQ)); EXPECT_EQ(1,transmitted(SENSOR,PAYLOAD_TYPE_ANON_REQ));
  EXPECT_EQ(1,transmitted(SENSOR,PAYLOAD_TYPE_TRACE)); EXPECT_TRUE(bot->diagnostics().login_path);
  EXPECT_EQ(TraceTimingPolicy::floodTimeout(LORA_REFERENCE[0].airtime[57]),bot->timing().login_wait); EXPECT_LE(bot->timing().duration,120000u);
  expectCleanPools();
}

TEST_F(TargetedSensorNetwork, AustraliaNarrowMultiFloodKeepsGuestLoginAndTraceLogicalOwnership) {
  for(auto& radio:radios) { radio.phy=&LORA_REFERENCE[0]; radio.phy_index=0; }
  relay(1).discovery_clients.load(&fs,relay(1).self_id);
  ASSERT_EQ(DoublerSettings::DEFAULTED,sensor_mesh->beginFlood(&fs));
  char reply[160]; ASSERT_TRUE(sensor_mesh->floodCommand("set multi.flood 2",reply)); ASSERT_STREQ("OK",reply);
  targetedRequest(1,0); runUntil(120000);
  ASSERT_EQ(1u,relay(0).messages.size()); EXPECT_NE(std::string::npos,relay(0).messages[0].find("avg SNR"));
  EXPECT_EQ(1u,relay(0).acknowledgements.size()); EXPECT_EQ(1u,bot->diagnostics().login_starts);
  EXPECT_EQ(1u,bot->diagnostics().starts); EXPECT_EQ(1,transmitted(SENSOR,PAYLOAD_TYPE_TRACE));
  EXPECT_TRUE(bot->diagnostics().login_path); EXPECT_EQ(0u,bot->diagnostics().report_failures);
  EXPECT_GT(sensor_mesh->floodStats.transmitted,0u); expectCleanPools();
}
