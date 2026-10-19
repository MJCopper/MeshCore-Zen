#pragma once

#include <vector>
#include <algorithm>
#include <Arduino.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/AdvertDataHelpers.h>
#include "SensorBotService.h"

class TestRadio : public mesh::Radio {
public:
  std::vector<uint8_t> incoming;
  std::vector<std::vector<uint8_t>> sent;
  bool complete = false;
  bool fail = false;
  unsigned start_calls = 0;
  uint32_t airtime = 100;
  int recvRaw(uint8_t* bytes, int size) override {
    int count = std::min((int)incoming.size(), size);
    memcpy(bytes, incoming.data(), count);
    incoming.clear();
    return count;
  }
  uint32_t getEstAirtimeFor(int) override { return airtime; }
  float packetScore(float, int) override { return 1; }
  bool startSendRaw(const uint8_t* bytes, int length) override {
    start_calls++;
    if (fail) return false;
    sent.emplace_back(bytes, bytes + length);
    complete = false;
    return true;
  }
  bool isSendComplete() override { return complete; }
  void onSendFinished() override { complete = false; }
  bool isInRecvMode() const override { return true; }
};

class TestClock : public mesh::MillisecondClock {
public:
  TestClock() { g_mock_millis = 0; }
  unsigned long getMillis() override { return g_mock_millis; }
};
class TestRTC : public mesh::RTCClock {
  uint32_t getCurrentTime() override { return 1800000000 + g_mock_millis / 1000; }
  void setCurrentTime(uint32_t) override { }
};
class TestRandom : public mesh::RNG {
  uint8_t _value = 0;
public:
  void seed(uint8_t value) { _value = value; }
private:
  void random(uint8_t* dest, size_t length) override {
    for (size_t i = 0; i < length; i++) dest[i] = ++_value;
  }
};

// The integration adapter uses real MeshCore encryption, parsing, dispatcher and pool.
class TestMesh : public mesh::Mesh, public SensorBotTransport {
public:
  SensorBotService* bot = nullptr;
  void loop() { if (bot) bot->beforeRadioLoop(); Mesh::loop(); }
  uint32_t profile = 0;
  uint8_t reject_direct = 0, reject_group = 0;
  bool definite_start_failure = true;
  TestMesh(TestRadio& radio, TestClock& clock, TestRandom& rng, TestRTC& rtc,
            StaticPoolPacketManager& pool, SimpleMeshTables& tables)
      : Mesh(radio, clock, rng, rtc, pool, tables) { }
  uint32_t now() override { return g_mock_millis; }
  uint32_t uniqueTime() override { return getRTCClock()->getCurrentTimeUnique(); }
  const char* nodeName() override { return "BME680 Sensor"; }
  mesh::RNG& rng() override { return *getRNG(); }
  uint32_t estimateAirtime(uint8_t length) override { return _radio->getEstAirtimeFor(length); }
  uint32_t radioProfile() override { return profile; }
  bool failedStartDefinitelyNotSent() const override { return definite_start_failure; }
  mesh::Packet* createTrace(uint32_t tag, uint32_t auth, uint8_t flags) override {
    return Mesh::createTrace(tag, auth, flags);
  }
  mesh::Packet* createGroupDatagram(uint8_t type, const mesh::GroupChannel& channel,
                                   const uint8_t* data, size_t length) override {
    return Mesh::createGroupDatagram(type, channel, data, length);
  }
  void sendFlood(mesh::Packet* packet, uint32_t delay) override {
    if (reject_group) { reject_group--; releasePacket(packet); }
    else Mesh::sendFlood(packet, delay);
  }
  void sendDirect(mesh::Packet* packet, const uint8_t* path, uint8_t length, uint32_t delay) override {
    if (reject_direct) { reject_direct--; releasePacket(packet); }
    else Mesh::sendDirect(packet, path, length, delay);
  }
  void releasePacket(mesh::Packet* packet) override { Mesh::releasePacket(packet); }
  bool isQueuedPacket(const mesh::Packet* packet) const override { return Mesh::isQueuedPacket(packet); }
  bool cancelQueuedPacket(mesh::Packet* packet) override { return Mesh::cancelQueuedPacket(packet); }
protected:
  bool allowPacketForward(const mesh::Packet*) override { return false; }
  void logRx(mesh::Packet* packet, int, float) override { if (bot) bot->observeRoute(packet); }
  int calcRxDelay(float, uint32_t) const override { return 0; }
  void onTxStarted(mesh::Packet* packet, uint32_t time) override { if (bot) bot->onTxStarted(packet, time); }
  void logTx(mesh::Packet* packet, int length) override { if (bot) bot->logTx(packet, length); }
  void logTxFail(mesh::Packet* packet, int length) override { if (bot) bot->logTxFail(packet, length); }
  int searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int max) override {
    return bot ? bot->searchChannelsByHash(hash, channels, max) : 0;
  }
  void onGroupDataRecv(mesh::Packet* packet, uint8_t type, const mesh::GroupChannel& channel,
                       uint8_t* data, size_t length) override {
    bot->onGroupDataRecv(packet, type, channel, data, length);
  }
  void onTraceRecv(mesh::Packet* packet, uint32_t tag, uint32_t auth, uint8_t flags,
                   const uint8_t* snrs, const uint8_t* path, uint8_t length) override {
    bot->onTraceRecv(packet, tag, auth, flags, snrs, path, length);
  }
  void onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id, uint32_t timestamp,
                     const uint8_t* data, size_t length) override {
    bot->onAdvertRecv(packet, id, timestamp, data, length);
  }
};
