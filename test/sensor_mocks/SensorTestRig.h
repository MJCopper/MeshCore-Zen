#pragma once

#include <vector>
#include <algorithm>
#include <Arduino.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/AdvertDataHelpers.h>
#include "SensorScopeBinding.h"
#include <helpers/RegionScope.h>
#include <helpers/ClientACL.h>

class TestRadio : public mesh::Radio {
public:
  std::vector<uint8_t> incoming;
  std::vector<std::vector<uint8_t>> sent;
  bool complete = false;
  bool fail = false;
  bool busy = false;
  bool transmitting = false;
  unsigned estimates_during_tx = 0;
  unsigned start_calls = 0;
  uint32_t airtime = 100;
  float last_snr = 0;
  int recvRaw(uint8_t* bytes, int size) override {
    int count = std::min((int)incoming.size(), size);
    memcpy(bytes, incoming.data(), count);
    incoming.clear();
    return count;
  }
  uint32_t getEstAirtimeFor(int) override {
    if (transmitting) estimates_during_tx++;
    return airtime;
  }
  float packetScore(float, int) override { return 1; }
  float getLastSNR() const override { return last_snr; }
  bool startSendRaw(const uint8_t* bytes, int length) override {
    start_calls++;
    if (fail) return false;
    sent.emplace_back(bytes, bytes + length);
    complete = false; transmitting = true;
    return true;
  }
  bool isSendComplete() override { return complete; }
  void onSendFinished() override { complete = false; transmitting = false; }
  bool isReceiving() override { return busy; }
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

// Native runtime uses real region/key resolution and the production scope binding.
class ScopeTestRuntime : public mesh::Mesh {
public:
  using mesh::Mesh::Mesh;
  TransportKeyStore scope_store;
  RegionMap regions{scope_store};
  TransportKey default_scope{};
  ClientACL management_acl;
  unsigned management_path_updates = 0, management_data_calls = 0;
  int matched_clients[MAX_CLIENTS]{};
protected:
  int searchPeersByHash(const uint8_t* hash) override {
    int count = 0;
    for (int i = 0; i < management_acl.getNumClients() && count < 24; i++)
      if (management_acl.getClientByIdx(i)->id.isHashMatch(hash)) matched_clients[count++] = i;
    return count;
  }
  void getPeerSharedSecret(uint8_t* secret, int index) override {
    memcpy(secret, management_acl.getClientByIdx(matched_clients[index])->shared_secret, 32);
  }
  void onPeerDataRecv(mesh::Packet*, uint8_t, int, const uint8_t*, uint8_t*, size_t) override {
    management_data_calls++;
  }
  bool onPeerPathRecv(mesh::Packet*, int index, const uint8_t*, uint8_t* path,
                       uint8_t length, uint8_t, uint8_t*, uint8_t) override {
    auto client = management_acl.getClientByIdx(matched_clients[index]);
    client->out_path_len = mesh::Packet::copyPath(client->out_path, path, length);
    management_path_updates++;
    return false;
  }
  mesh::PacketScope captureRequestScope(const mesh::Packet& packet) {
    return mesh::resolvePacketScope(packet, regions);
  }
  TransportKey captureReplyScope(const mesh::Packet& packet, const mesh::PacketScope& scope) {
    return mesh::captureReplyTransportScope(packet, scope, default_scope);
  }
  void sendCapturedReply(const TransportKey& scope, mesh::Packet* packet, uint32_t delay, uint8_t width) {
    mesh::sendCapturedFlood(static_cast<mesh::Mesh&>(*this), scope, packet, delay, width);
  }
};

// The integration adapter uses real MeshCore encryption, parsing, dispatcher and pool.
class TestMesh : public SensorScopeBinding<ScopeTestRuntime> {
public:
  SensorBotService* bot = nullptr;
  void loop() { if (bot) bot->beforeRadioLoop(); Mesh::loop(); }
  const char* node_name = "BME680 Sensor";
  uint32_t profile = 0;
  uint32_t cad_max_duration = 4000;
  uint8_t reject_direct = 0, reject_group = 0;
  bool definite_start_failure = true;
  // Legacy phase-boundary tests isolate probe/report timing. New acknowledgement
  // tests and network tests leave this false and use the real Dispatcher.
  bool complete_trace_ack_immediately = false;
  std::vector<std::string> isolated_acknowledgements;
  mesh::Packet* isolated_ack_packet = nullptr;
  TestMesh(TestRadio& radio, TestClock& clock, TestRandom& rng, TestRTC& rtc,
            StaticPoolPacketManager& pool, SimpleMeshTables& tables)
      : SensorScopeBinding<ScopeTestRuntime>(radio, clock, rng, rtc, pool, tables) { }
  uint32_t now() override { return g_mock_millis; }
  uint32_t uniqueTime() override { return getRTCClock()->getCurrentTimeUnique(); }
  const char* nodeName() override { return node_name; }
  mesh::RNG& rng() override { return *getRNG(); }
  uint32_t estimateAirtime(uint8_t length) override { return _radio->getEstAirtimeFor(length); }
  uint32_t radioProfile() override { return profile; }
  bool failedStartDefinitelyNotSent() const override { return definite_start_failure; }
  mesh::Packet* createTrace(uint32_t tag, uint32_t auth, uint8_t flags) override {
    return Mesh::createTrace(tag, auth, flags);
  }
  mesh::Packet* createGroupDatagram(uint8_t type, const mesh::GroupChannel& channel,
                                   const uint8_t* data, size_t length) override {
    auto packet = Mesh::createGroupDatagram(type, channel, data, length);
    if (packet && complete_trace_ack_immediately && length > 5 &&
        std::string((const char*)data + 5, length - 5).find("Trace requested, please wait up to ") != std::string::npos) {
      isolated_ack_packet = packet;
      isolated_acknowledgements.emplace_back((const char*)data + 5, length - 5);
    }
    return packet;
  }
  void sendReply(mesh::Packet* packet, uint32_t delay, const SensorReplyContext& context) override {
    if (packet == isolated_ack_packet) {
      isolated_ack_packet = nullptr;
      if (bot) { bot->onTxStarted(packet, now()); bot->logTx(packet, 0); }
      releasePacket(packet);
    } else if (reject_group) { reject_group--; releasePacket(packet); }
    else SensorScopeBinding<ScopeTestRuntime>::sendReply(packet, delay, context);
  }
  void sendDirect(mesh::Packet* packet, const uint8_t* path, uint8_t length, uint32_t delay) override {
    if (reject_direct) { reject_direct--; releasePacket(packet); }
    else Mesh::sendDirect(packet, path, length, delay);
  }
  void releasePacket(mesh::Packet* packet) override { Mesh::releasePacket(packet); }
  bool isQueuedPacket(const mesh::Packet* packet) const override { return Mesh::isQueuedPacket(packet); }
  bool cancelQueuedPacket(mesh::Packet* packet) override { return Mesh::cancelQueuedPacket(packet); }
protected:
  uint32_t getCADFailMaxDuration() const override { return cad_max_duration; }
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
