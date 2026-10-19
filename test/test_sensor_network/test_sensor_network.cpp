#include <gtest/gtest.h>
#include <deque>
#include <memory>
#include "../sensor_mocks/SensorTestRig.h"

// Deterministic packet medium: real MeshCore forwarding, simulated links and airtime.
// Optional overlap/capture/half-duplex model is deliberately approximate, not RF acceptance.
class NetworkRadio : public TestRadio {
public:
  std::deque<std::vector<uint8_t>> received;
  uint32_t started_at = 0;
  bool active = false;
  uint32_t busy_until = 0;
  bool isReceiving() override { return g_mock_millis < busy_until; }
  int recvRaw(uint8_t* bytes, int size) override {
    if (received.empty()) return 0;
    auto packet = received.front();
    received.pop_front();
    int length = std::min(size, (int)packet.size());
    memcpy(bytes, packet.data(), length);
    return length;
  }
  bool startSendRaw(const uint8_t* bytes, int length) override {
    if (!TestRadio::startSendRaw(bytes, length)) return false;
    started_at = g_mock_millis;
    active = true;
    return true;
  }
  bool isSendComplete() override { return active && g_mock_millis - started_at >= airtime; }
  void onSendFinished() override { active = false; }
};

class NetworkRelay : public mesh::Mesh {
public:
  bool forwards;
  PublicChannelSensorBot channels;
  std::vector<std::string> messages;
  uint32_t direct_delay = 0, flood_delay = 0;
  bool standard_delay = false;
  NetworkRelay(NetworkRadio& radio, TestClock& clock, TestRandom& random, TestRTC& rtc,
               StaticPoolPacketManager& pool, SimpleMeshTables& tables, bool forward)
      : Mesh(radio, clock, random, rtc, pool, tables), forwards(forward) { }
protected:
  int searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel dest[], int capacity) override {
    return channels.findChannel(hash, dest, capacity);
  }
  void onGroupDataRecv(mesh::Packet*, uint8_t, const mesh::GroupChannel&,
                       uint8_t* data, size_t length) override {
    if (length > 5) messages.emplace_back((char*)data + 5, strnlen((char*)data + 5, length - 5));
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
  struct Delivery { uint32_t due; int target; std::vector<uint8_t> bytes;
                    uint32_t start; int power; bool lost; };
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
      Delivery delivery{end, target, bytes, now, power[source][target], false};
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
          radios[pending[i].target].received.push_back(pending[i].bytes);
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
  void request(uint8_t width = 1, uint8_t channel_index = 0) {
    uint8_t payload[64]{};
    uint32_t time = 1800000000;
    memcpy(payload, &time, 4);
    const char* text = "Alice: !hillvue trace";
    strcpy((char*)payload + 5, text);
    auto packet = meshes[0]->createGroupDatagram(PAYLOAD_TYPE_GRP_TXT, channel.channel(channel_index),
                                                payload, 5 + strlen(text));
    ASSERT_NE(nullptr, packet);
    meshes[0]->sendFlood(packet, 0, width);
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
      EXPECT_EQ(channel.channel(channel_index).hash[0], bytes[2]);
      int length = mesh::Utils::MACThenDecrypt(channel.channel(channel_index).secret, data,
                                              bytes.data() + 3, bytes.size() - 3);
      EXPECT_GT(length, 5);
      result.emplace_back((char*)data + 5, strnlen((char*)data + 5, length > 5 ? length - 5 : 0));
    }
    return result;
  }
  void expectCleanPools() {
    for (auto& pool : pools) EXPECT_EQ(32, pool->getFreeCount());
    EXPECT_EQ(0u, fs.writes);
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
    EXPECT_EQ(1, transmitted(SENSOR, PAYLOAD_TYPE_TRACE));
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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
