#include <gtest/gtest.h>
#include <deque>
#include <memory>
#include "../sensor_mocks/SensorTestRig.h"

// Deterministic packet medium: real MeshCore forwarding, simulated links and airtime.
// It does not model RF capture, interference or hardware sensor behaviour.
class NetworkRadio : public TestRadio {
public:
  std::deque<std::vector<uint8_t>> received;
  uint32_t started_at = 0;
  bool active = false;
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
  uint32_t getDirectRetransmitDelay(const mesh::Packet*) override { return direct_delay; }
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
  struct Delivery { uint32_t due; int target; std::vector<uint8_t> bytes; };
  std::vector<Delivery> pending;

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
        radios[pending[i].target].received.push_back(pending[i].bytes);
        pending.erase(pending.begin() + i);
      }
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
          for (int target = 0; target < COUNT; target++)
            if (links[source][target]) pending.push_back({now + radios[source].airtime, target, bytes});
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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
