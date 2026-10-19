#include <gtest/gtest.h>
#include "../sensor_mocks/SensorTestRig.h"

class SensorIntegration : public ::testing::Test {
protected:
  TestRadio radio;
  TestClock clock;
  TestRTC rtc;
  TestRandom random;
  StaticPoolPacketManager pool{32};
  SimpleMeshTables tables;
  TestMesh mesh{radio, clock, random, rtc, pool, tables};
  Bme680Telemetry snapshot;
  SensorBotService bot{mesh, snapshot};
  MemoryFileSystem fs;
  PublicChannelSensorBot channel;

  void SetUp() override {
    g_mock_millis = 0;
    mesh.bot = &bot;
    mesh.begin();
    bot.begin(&fs);
  }
  void tick(uint32_t now) { g_mock_millis = now; mesh.loop(); bot.loop(); }
  void finish(uint32_t now) { radio.complete = true; tick(now); }
  std::vector<uint8_t> command(const char* text, uint8_t hops = 2,
                              const mesh::GroupChannel* selected = nullptr) {
    uint8_t data[MAX_PACKET_PAYLOAD] = {};
    uint32_t time = mesh.uniqueTime();
    memcpy(data, &time, 4);
    strcpy((char*)data + 5, text);
    auto packet = mesh.createGroupDatagram(PAYLOAD_TYPE_GRP_TXT, selected ? *selected : channel.channel(), data,
                                            5 + strlen(text));
    packet->header |= ROUTE_TYPE_FLOOD;
    packet->setPathHashSizeAndCount(1, hops);
    for (uint8_t i = 0; i < hops; i++) packet->path[i] = 20 + i;
    std::vector<uint8_t> bytes{packet->header, (uint8_t)packet->path_len};
    bytes.insert(bytes.end(), packet->path, packet->path + hops);
    bytes.insert(bytes.end(), packet->payload, packet->payload + packet->payload_len);
    mesh.releasePacket(packet);
    return bytes;
  }
  std::string publicText(size_t index, const mesh::GroupChannel* selected = nullptr) {
    auto& bytes = radio.sent.at(index);
    const auto& reply_channel = selected ? *selected : channel.channel();
    EXPECT_EQ(reply_channel.hash[0], bytes[2]);
    uint8_t data[MAX_PACKET_PAYLOAD] = {};
    int count = mesh::Utils::MACThenDecrypt(reply_channel.secret, data,
                                            bytes.data() + 3, bytes.size() - 3);
    EXPECT_GT(count, 5);
    return std::string((char*)data + 5, strnlen((char*)data + 5, count > 5 ? count - 5 : 0));
  }
  void traceReply(size_t index, uint32_t now) {
    const auto trace = radio.sent.at(index);
    uint8_t count = trace.size() - 2 - 9;
    radio.incoming = { uint8_t((PAYLOAD_TYPE_TRACE << PH_TYPE_SHIFT) | ROUTE_TYPE_DIRECT), count };
    for (uint8_t i = 0; i < count; i++) radio.incoming.push_back(4 * (i + 1));
    radio.incoming.insert(radio.incoming.end(), trace.begin() + 2, trace.end());
    tick(now);
  }
};

TEST_F(SensorIntegration, PublicPingRoundTripsThroughRealEncryptionAndPool) {
  radio.incoming = command("Alice: !hillvue ping");
  tick(100);
  EXPECT_TRUE(radio.sent.empty());
  tick(2200);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Pong", publicText(0));
  finish(2300);
  EXPECT_EQ(32, pool.getFreeCount());
  EXPECT_EQ(0u, fs.writes);
}

TEST_F(SensorIntegration, WatchesHillvueAndPublicAndRepliesOnTheRequestChannel) {
  // First 16 bytes of SHA256("#hillvue"), independently calculated.
  const uint8_t expected[] = { 0xd7, 0x83, 0x3b, 0xdb, 0x8c, 0x55, 0x6b, 0xca,
    0x33, 0xdf, 0x33, 0x20, 0x63, 0xf6, 0xe2, 0x43 };
  EXPECT_EQ(0, memcmp(expected, channel.channel().secret, sizeof(expected)));
  PublicChannelSensorBot public_channel("");
  radio.incoming = command("Alice: !hillvue ping", 2, &public_channel.channel());
  tick(100);
  tick(2200);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Pong", publicText(0, &public_channel.channel()));
  finish(2250);
  radio.incoming = command("Alice: !hillvue ping");
  tick(2300);
  tick(4500);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Pong", publicText(1));
  finish(4600);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorIntegration, CombinedReadingsUseCachedValuesAndExistingFormatting) {
  CayenneLPP raw(180);
  raw.addTemperature(2, 22.4f);
  raw.addRelativeHumidity(2, 48);
  raw.addBarometricPressure(2, 1013.2f);
  raw.addAltitude(2, 300);
  raw.addGenericSensor(2, 100000);
  ASSERT_TRUE(snapshot.ingest(raw));
  radio.incoming = command("Alice: !hillvue t air");
  tick(100);
  tick(2200);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Temperature: 22.4 C\nAir Quality: Warming Up (1/10)", publicText(0));
  EXPECT_EQ(1, snapshot.samples());
  EXPECT_EQ(0u, fs.writes);
}

TEST_F(SensorIntegration, InvalidAndUnrelatedCommandsNeverTransmit) {
  for (const char* command_text : { "Alice: hello", "Alice: !hillvue unknown", "Alice: !hillvue ping air" }) {
    radio.incoming = command(command_text);
    tick(g_mock_millis + 100);
    tick(g_mock_millis + 2200);
  }
  EXPECT_TRUE(radio.sent.empty());
  EXPECT_EQ(32, pool.getFreeCount());
  EXPECT_EQ(0u, fs.writes);
}

TEST_F(SensorIntegration, DuplicateOriginalCommandDoesNotPostBusyNotice) {
  auto bytes = command("Alice: !hillvue trace");
  radio.incoming = bytes;
  tick(100);
  ASSERT_EQ(1u, radio.sent.size());
  finish(200);
  radio.incoming = bytes;
  tick(300);
  EXPECT_EQ(0, pool.getOutboundTotal());
  EXPECT_EQ(1u, radio.sent.size());
}

TEST_F(SensorIntegration, RetryIsSilentUntilOneFinalFailure) {
  radio.incoming = command("Alice: !hillvue trace");
  tick(100);
  finish(200);
  tick(10100);
  EXPECT_EQ(1u, radio.sent.size());
  tick(12200);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ(PAYLOAD_TYPE_TRACE, (radio.sent[1][0] >> PH_TYPE_SHIFT) & PH_TYPE_MASK);
  finish(12300);
  tick(22200);
  tick(25200);
  tick(28000);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: no result after retry", publicText(2));
  finish(28100);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorIntegration, LateFirstCancelsRetryBeforeItTransmits) {
  radio.incoming = command("Alice: !hillvue trace");
  tick(100);
  finish(200);
  tick(10100);
  traceReply(0, 10500);
  tick(14000);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(1).find("Trace: 10400 ms RTT"));
  finish(14100);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorIntegration, LateFirstIsUsedWhenRetryTimesOut) {
  radio.incoming = command("Alice: !hillvue trace");
  tick(100);
  finish(200);
  tick(10100);
  tick(12200);
  finish(12300);
  traceReply(0, 13000);
  EXPECT_EQ(2u, radio.sent.size());
  tick(22200);
  tick(25000);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(2).find("Trace: 12900 ms RTT"));
}

TEST_F(SensorIntegration, MultipartGapStartsAtFirstTxCompletion) {
  std::string text;
  for (int i = 0; i < 8; i++) text += "Temperature: 22.4 C and more data\n";
  bot.sendPublicResponse(text.c_str(), 0);
  tick(2200);
  ASSERT_EQ(1u, radio.sent.size());
  finish(5000);
  tick(7999);
  EXPECT_EQ(1u, radio.sent.size());
  tick(8000);
  EXPECT_EQ(1, pool.getOutboundTotal());
  tick(8001);
  EXPECT_EQ(2u, radio.sent.size());
  finish(8100);
  tick(20000);
  EXPECT_EQ(2u, radio.sent.size());
}

TEST_F(SensorIntegration, BothChannelsShareOneRollingCommandLimit) {
  for (uint8_t i = 0; i < 5; i++) {
    uint32_t start = i * 3000;
    const auto& source = channel.channel(i % 2);
    radio.incoming = command("Alice: !hillvue ping", 2, &source);
    tick(start + 100);
    tick(start + 2200);
    ASSERT_EQ((size_t)std::min(i + 1, 4), radio.sent.size());
    if (i < 4) EXPECT_EQ("BME680 Sensor: Pong", publicText(i, &source));
    finish(start + 2300);
  }
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorIntegration, UnwatchedHashtagDoesNotGenerateAReply) {
  PublicChannelSensorBot other("#other");
  radio.incoming = command("Alice: !hillvue ping", 2, &other.channel());
  tick(100);
  tick(2200);
  EXPECT_TRUE(radio.sent.empty());
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorIntegration, MultipartRepliesKeepTheirOwnChannelsWhenInterleaved) {
  std::string text;
  for (int i = 0; i < 8; i++) text += "Temperature: 22.4 C and more data\n";
  bot.sendPublicResponse(text.c_str(), 0);
  bot.sendPublicResponse(text.c_str(), 1);
  for (uint32_t now = 100; now <= 10000; now += 100) {
    radio.complete = true;
    tick(now);
  }
  ASSERT_EQ(4u, radio.sent.size());
  for (uint8_t id = 0; id < 2; id++) {
    int parts = 0;
    for (size_t i = 0; i < radio.sent.size(); i++) {
      if (radio.sent[i][2] != channel.channel(id).hash[0]) continue;
      auto text = publicText(i, &channel.channel(id));
      EXPECT_NE(std::string::npos, text.find(parts == 0 ? "1/2\n" : "2/2\n"));
      parts++;
    }
    EXPECT_EQ(2, parts);
  }
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorIntegration, PublicTraceResultSurvivesInterveningHillvueRequest) {
  radio.incoming = command("Alice: !hillvue trace", 2, &channel.channel(1));
  tick(100);
  finish(200);
  radio.incoming = command("Bob: !hillvue ping");
  tick(300);
  tick(2400);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Pong", publicText(1));
  finish(2500);
  traceReply(0, 3000);
  tick(6500);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(2, &channel.channel(1)).find("Trace: 2900 ms RTT"));
  finish(6600);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorIntegration, BusyNoticeUsesNewRequestChannelNotActiveTraceChannel) {
  radio.incoming = command("Alice: !hillvue trace", 2, &channel.channel(1));
  tick(100);
  finish(200);
  radio.incoming = command("Bob: !hillvue trace");
  tick(300);
  tick(2400);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: already running", publicText(1));
  finish(2500);
  traceReply(0, 3000);
  tick(6500);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(2, &channel.channel(1)).find("Trace: 2900 ms RTT"));
}

TEST_F(SensorIntegration, PublicTraceFallbackSurvivesInterveningHillvueRequest) {
  radio.incoming = command("Alice: !hillvue trace", 2, &channel.channel(1));
  tick(100);
  finish(200);
  tick(10100);
  tick(12200);
  finish(12300);
  radio.incoming = command("Bob: !hillvue ping");
  tick(12400);
  tick(14500);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Pong", publicText(2));
  finish(14600);
  traceReply(0, 15000);
  tick(22200);
  tick(25000);
  ASSERT_EQ(4u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(3, &channel.channel(1)).find("Trace: 14900 ms RTT"));
}

TEST_F(SensorIntegration, AdvertTxTimeoutReleasesPacketAndRadioContinues) {
  mesh::LocalIdentity identity(&random);
  uint8_t app[MAX_ADVERT_DATA_SIZE];
  uint8_t length = AdvertDataBuilder(ADV_TYPE_SENSOR, "BME680 Sensor").encodeTo(app);
  auto advert = mesh.Mesh::createAdvert(identity, app, length);
  ASSERT_NE(nullptr, advert);
  mesh.sendFlood(advert, 1500);
  tick(1500);
  ASSERT_EQ(1u, radio.sent.size());
  tick(1700);  // Deliberately withhold TX-complete and exercise Dispatcher recovery.
  EXPECT_EQ(32, pool.getFreeCount());
  radio.incoming = command("Alice: !hillvue ping");
  tick(1800);
  tick(4000);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Pong", publicText(1));
}

TEST_F(SensorIntegration, OnlyVerifiedRepeaterAdvertsEnterNameCache) {
  mesh::LocalIdentity id(&random);
  uint8_t app[MAX_ADVERT_DATA_SIZE];
  uint8_t length = AdvertDataBuilder(ADV_TYPE_REPEATER, "Ridge").encodeTo(app);
  auto packet = mesh.Mesh::createAdvert(id, app, length);
  ASSERT_NE(nullptr, packet);
  std::vector<uint8_t> bytes{ uint8_t((PAYLOAD_TYPE_ADVERT << PH_TYPE_SHIFT) | ROUTE_TYPE_FLOOD), 0 };
  bytes.insert(bytes.end(), packet->payload, packet->payload + packet->payload_len);
  mesh.releasePacket(packet);
  radio.incoming = bytes;
  tick(100);
  radio.incoming = command("Alice: !hillvue path", 1);
  radio.incoming[2] = id.pub_key[0];
  tick(200);
  tick(2400);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(0).find("Ridge"));
  finish(2500);
  bytes.back() ^= 1;  // Invalid signature must not replace the verified name.
  radio.incoming = bytes;
  tick(2600);
  EXPECT_EQ(0, pool.getOutboundTotal());
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorIntegration, FirstResultAfterDeadlineBeforePollingAvoidsRetry) {
  radio.incoming = command("Alice: !hillvue trace");
  tick(100);
  finish(200);
  traceReply(0, 10101);
  tick(14000);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(1).find("Trace: 10001 ms RTT"));
  finish(14100);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorIntegration, RetryResultAfterDeadlineBeforePollingAvoidsFailure) {
  radio.incoming = command("Alice: !hillvue trace");
  tick(100);
  finish(200);
  tick(10100);
  tick(12200);
  finish(12300);
  traceReply(1, 22201);
  tick(26000);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(2).find("Trace: 10001 ms RTT"));
}

TEST_F(SensorIntegration, LateRetryDuringGraceStillSucceeds) {
  radio.incoming = command("Alice: !hillvue trace");
  tick(100);
  finish(200);
  tick(10100);
  tick(12200);
  finish(12300);
  tick(22200);
  traceReply(1, 24000);
  tick(28000);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(2).find("Trace: 11800 ms RTT"));
}

TEST_F(SensorIntegration, LateResultReplacesQueuedFailureWithoutLeakingItsPacket) {
  radio.incoming = command("Alice: !hillvue trace", 2, &channel.channel(1));
  tick(100);
  finish(200);
  tick(10100);
  tick(12200);
  finish(12300);
  tick(22200);
  tick(25200);
  EXPECT_EQ(1, pool.getOutboundTotal());
  traceReply(1, 25201);
  tick(29000);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(2, &channel.channel(1)).find("Trace: 13001 ms RTT"));
  finish(29100);
  tick(40000);
  EXPECT_EQ(3u, radio.sent.size());
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorIntegration, CommittedFailureCannotBecomeASecondTerminalReply) {
  radio.incoming = command("Alice: !hillvue trace");
  tick(100);
  finish(200);
  tick(10100);
  tick(12200);
  finish(12300);
  tick(22200);
  tick(25200);
  tick(28000);
  ASSERT_EQ(3u, radio.sent.size());
  finish(28100);
  traceReply(0, 28200);
  tick(32000);
  EXPECT_EQ(3u, radio.sent.size());
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorIntegration, OldResultDoesNotCompleteANewerTraceAfterPacketReuse) {
  radio.incoming = command("Alice: !hillvue trace");
  tick(100);
  finish(200);
  traceReply(0, 1000);
  tick(4500);
  finish(4600);
  radio.incoming = command("Alice: !hillvue trace");
  tick(5000);
  ASSERT_EQ(3u, radio.sent.size());
  finish(5100);
  traceReply(0, 5200);
  EXPECT_EQ(0, pool.getOutboundTotal());
  traceReply(2, 6000);
  tick(9500);
  ASSERT_EQ(4u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(3).find("Trace: 1000 ms RTT"));
}

TEST_F(SensorIntegration, FailureAllocationExhaustionClosesWithinTheReportBudget) {
  radio.incoming = command("Alice: !hillvue trace");
  tick(100);
  finish(200);
  tick(10100);
  tick(12200);
  finish(12300);
  std::vector<mesh::Packet*> held;
  while (auto packet = pool.allocNew()) held.push_back(packet);
  tick(22200);
  tick(25200);
  tick(35200);
  for (auto packet : held) pool.free(packet);
  radio.incoming = command("Bob: !hillvue trace");
  tick(35300);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_EQ(PAYLOAD_TYPE_TRACE, (radio.sent[2][0] >> PH_TYPE_SHIFT) & PH_TYPE_MASK);
}

TEST_F(SensorIntegration, RetryUsesAPassivelyObservedAlternateRequestPath) {
  auto first_command = command("Alice: !hillvue trace");
  radio.incoming = first_command;
  tick(100);
  finish(200);
  first_command[2] = 40;
  first_command[3] = 41;
  radio.incoming = first_command;
  tick(300);
  EXPECT_EQ(1u, radio.sent.size());
  tick(10100);
  tick(12200);
  ASSERT_EQ(2u, radio.sent.size());
  const uint8_t expected[] = { 41, 40, 41 };
  EXPECT_EQ(0, memcmp(expected, radio.sent[1].data() + 11, sizeof(expected)));
  finish(12300);
  traceReply(1, 14000);
  tick(18000);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(2).find("1 29 +1.0 dB"));
}

TEST_F(SensorIntegration, AlternateRetryKeepsOriginalFallbackLabels) {
  auto bytes = command("Alice: !hillvue trace");
  radio.incoming = bytes;
  tick(100);
  finish(200);
  bytes[2] = 40;
  bytes[3] = 41;
  radio.incoming = bytes;
  tick(300);
  tick(10100);
  tick(12200);
  finish(12300);
  traceReply(0, 14000);
  tick(22200);
  tick(26000);
  ASSERT_EQ(3u, radio.sent.size());
  auto text = publicText(2);
  EXPECT_NE(std::string::npos, text.find("1 15 +1.0 dB"));
  EXPECT_EQ(std::string::npos, text.find("1 29 +1.0 dB"));
}

TEST_F(SensorIntegration, FailureThatNeverStartedCanStillBeReplacedAfterRadioError) {
  radio.incoming = command("Alice: !hillvue trace");
  tick(100);
  finish(200);
  tick(10100);
  tick(12200);
  finish(12300);
  tick(22200);
  tick(25200);
  radio.fail = true;
  tick(28000);
  ASSERT_EQ(2u, radio.sent.size());
  radio.fail = false;
  traceReply(0, 28100);
  tick(32000);
  ASSERT_EQ(3u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(2).find("Trace: 28000 ms RTT"));
  finish(32100);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorIntegration, UnsentFailureIsCancelledAtTheReportDeadline) {
  radio.incoming = command("Alice: !hillvue trace");
  tick(100);
  finish(200);
  tick(10100);
  tick(12200);
  finish(12300);
  tick(22200);
  tick(25200);
  ASSERT_EQ(1, pool.getOutboundTotal());
  // Advance the service without giving the radio a send opportunity.
  g_mock_millis = 35200;
  bot.loop();
  EXPECT_EQ(0, pool.getOutboundTotal());
  EXPECT_EQ(32, pool.getFreeCount());
  traceReply(0, 35300);
  tick(39000);
  EXPECT_EQ(2u, radio.sent.size());
}

TEST_F(SensorIntegration, RetryAllocationFailureStillAllowsLateFirstSuccess) {
  radio.incoming = command("Alice: !hillvue trace");
  tick(100);
  finish(200);
  std::vector<mesh::Packet*> held;
  while (auto packet = pool.allocNew()) held.push_back(packet);
  tick(10100);
  for (auto packet : held) pool.free(packet);
  traceReply(0, 11000);
  tick(15000);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(1).find("Trace: 10900 ms RTT"));
  finish(15100);
  EXPECT_EQ(32, pool.getFreeCount());
}

TEST_F(SensorIntegration, RetryStartFailureDoesNotDiscardLateFirstSuccess) {
  radio.incoming = command("Alice: !hillvue trace");
  tick(100);
  finish(200);
  tick(10100);
  radio.fail = true;
  tick(12200);
  radio.fail = false;
  traceReply(0, 12300);
  tick(16000);
  ASSERT_EQ(2u, radio.sent.size());
  EXPECT_NE(std::string::npos, publicText(1).find("Trace: 12200 ms RTT"));
}

TEST_F(SensorIntegration, InitialSendFailureIsNotReportedAsTwoExpiredProbes) {
  radio.fail = true;
  radio.incoming = command("Alice: !hillvue trace");
  tick(100);
  radio.fail = false;
  tick(3000);
  ASSERT_EQ(1u, radio.sent.size());
  EXPECT_EQ("BME680 Sensor: Trace: transmit failed", publicText(0));
  finish(3100);
  EXPECT_EQ(32, pool.getFreeCount());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
