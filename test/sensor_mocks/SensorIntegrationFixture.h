#pragma once
#include <gtest/gtest.h>
#include "../sensor_mocks/SensorTestRig.h"
#include <helpers/AdvertDataHelpers.h>

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
  SensorBotService bot{mesh, snapshot, false};  // Legacy exact-time boundary characterization.
  MemoryFileSystem fs;
  PublicChannelSensorBot channel;

  void SetUp() override {
    g_mock_millis = 0;
    mesh.complete_trace_ack_immediately = true;
    mesh.bot = &bot;
    mesh.begin();
    bot.begin(&fs);
  }
  void tick(uint32_t now) { g_mock_millis = now; mesh.loop(); bot.loop(); }
  void finish(uint32_t now) { radio.complete = true; tick(now); }
  std::vector<uint8_t> command(const char* text, uint8_t hops = 2,
                              const mesh::GroupChannel* selected = nullptr, uint8_t width = 1) {
    uint8_t data[MAX_PACKET_PAYLOAD] = {};
    uint32_t time = mesh.uniqueTime();
    memcpy(data, &time, 4);
    strcpy((char*)data + 5, text);
    auto packet = mesh.createGroupDatagram(PAYLOAD_TYPE_GRP_TXT, selected ? *selected : channel.channel(), data,
                                            5 + strlen(text));
    packet->header |= ROUTE_TYPE_FLOOD;
    packet->setPathHashSizeAndCount(width, hops);
    for (uint8_t i = 0; i < hops * width; i++) packet->path[i] = 20 + i;
    std::vector<uint8_t> bytes{packet->header, (uint8_t)packet->path_len};
    bytes.insert(bytes.end(), packet->path, packet->path + hops * width);
    bytes.insert(bytes.end(), packet->payload, packet->payload + packet->payload_len);
    mesh.releasePacket(packet);
    return bytes;
  }
  std::string publicText(size_t index, const mesh::GroupChannel* selected = nullptr) {
    const auto& bytes = radio.sent.at(index);
    mesh::Packet packet{};
    EXPECT_TRUE(mesh.tryParsePacket(&packet, bytes.data(), bytes.size()));
    const auto& reply_channel = selected ? *selected : channel.channel();
    EXPECT_EQ(reply_channel.hash[0], packet.payload[0]);
    uint8_t data[MAX_PACKET_PAYLOAD] = {};
    int count = mesh::Utils::MACThenDecrypt(reply_channel.secret, data,
                                            packet.payload + 1, packet.payload_len - 1);
    EXPECT_GT(count, 5);
    return std::string((char*)data + 5, strnlen((char*)data + 5, count > 5 ? count - 5 : 0));
  }
  void traceReply(size_t index, uint32_t now, const std::vector<int8_t>& samples = {}) {
    mesh::Packet trace{};
    const auto& bytes = radio.sent.at(index);
    ASSERT_TRUE(mesh.tryParsePacket(&trace, bytes.data(), bytes.size()));
    uint8_t count = (trace.payload_len - 9) / (1 << trace.payload[8]);
    if (!samples.empty()) {
      ASSERT_EQ(count + 1u, samples.size());
      radio.last_snr = samples.back() / 4.0f;
    }
    radio.incoming = { uint8_t((PAYLOAD_TYPE_TRACE << PH_TYPE_SHIFT) | ROUTE_TYPE_DIRECT), count };
    for (uint8_t i = 0; i < count; i++) radio.incoming.push_back(samples.empty() ? 4 * (i + 1) : (uint8_t)samples[i]);
    radio.incoming.insert(radio.incoming.end(), trace.payload, trace.payload + trace.payload_len);
    tick(now);
    radio.last_snr = 0;
  }
};

