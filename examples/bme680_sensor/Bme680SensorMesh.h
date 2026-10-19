#pragma once

#include "../simple_sensor/SensorMesh.h"
#include "SensorBotService.h"
#include "SensorManagementPolicy.h"

// Thin binding between the upstream sensor runtime and this application's services.
class Bme680SensorMesh : public SensorMesh, private SensorBotTransport {
  SensorBotService _bot;
  TimeSeriesData _battery{12 * 24, 5 * 60};
  Trigger _low_batt, _critical_batt;
  CayenneLPP _cached_reply{MAX_PACKET_PAYLOAD - 4};
public:
  Bme680SensorMesh(mesh::MainBoard& board, mesh::Radio& radio, mesh::MillisecondClock& ms,
                   mesh::RNG& rng, mesh::RTCClock& rtc, mesh::MeshTables& tables)
      : SensorMesh(board, radio, ms, rng, rtc, tables), _bot(*this, sensors.snapshot) { }
  void begin(FILESYSTEM* fs) {
    SensorMesh::begin(fs);
    sensors.snapshot.setVoltage(board.getBattMilliVolts() / 1000.0f);
    _bot.begin(fs);
  }
  void loop() { SensorMesh::loop(); _bot.loop(); }

protected:
  void onSensorDataRead() override {
    float voltage = getVoltage(TELEM_CHANNEL_SELF);
    sensors.snapshot.setVoltage(voltage);
    _battery.recordData(getRTCClock(), voltage);
    alertIf(voltage < 3.4f, _critical_batt, HIGH_PRI_ALERT, "Battery is critical!");
    alertIf(voltage < 3.6f, _low_batt, LOW_PRI_ALERT, "Battery is low");
  }
  int querySeriesData(uint32_t start, uint32_t end, MinMaxAvg dest[], int) override {
    _battery.calcMinMaxAvg(getRTCClock(), start, end, dest, TELEM_CHANNEL_SELF, LPP_VOLTAGE);
    return 1;
  }
  bool handleCustomCommand(uint32_t, char* command, char* reply) override {
    if (strcmp(command, "get repeat") == 0) { strcpy(reply, "> off"); return true; }
    if (strncmp(command, "set repeat ", 11) == 0) {
      strcpy(reply, "OK - repeat is fixed OFF"); return true;
    }
    return false;
  }
  bool allowPacketForward(const mesh::Packet*) override { return false; }
  void onPrefsLoaded(NodePrefs& prefs) override { prefs.disable_fwd = true; }
  ClientInfo* authenticateClient(ClientACL& clients, const mesh::Identity& sender,
                                 const uint8_t* secret, uint32_t timestamp,
                                 const char* password, bool& persist) override {
    return SensorManagementPolicy::login(clients, sender, secret, timestamp,
        getRTCClock()->getCurrentTime(), password, getNodePrefs()->password, persist);
  }
  uint8_t loginMode(const ClientInfo& client) const override {
    return SensorManagementPolicy::loginMode(client);
  }
  uint8_t firmwareLevel() const override { return 2; }
  ClientSaveFilter clientSaveFilter() const override {
    return SensorManagementPolicy::persistClient;
  }
  int handleCustomRequest(uint8_t, uint8_t type, const uint8_t* payload, size_t length,
                          uint8_t* reply, size_t capacity) override {
    if (type == 3) {
      if (length < 1) return 0;
      sensors.snapshot.encode(_cached_reply, (uint8_t)~payload[0]);
      if (_cached_reply.getSize() > capacity) return 0;
      memcpy(reply, _cached_reply.getBuffer(), _cached_reply.getSize());
      return _cached_reply.getSize();
    }
    if (type == 7) return SensorManagementPolicy::ownerInfo(FIRMWARE_VERSION,
        getNodeName(), getNodePrefs()->owner_info, reply, capacity);
    if (type != 1) return -1;
    SensorStats stats = {};
    stats.batt_milli_volts = board.getBattMilliVolts();
    stats.curr_tx_queue_len = _mgr->getOutboundTotal();
    stats.noise_floor = _radio->getNoiseFloor();
    stats.last_rssi = radio_driver.getLastRSSI();
    stats.n_packets_recv = radio_driver.getPacketsRecv();
    stats.n_packets_sent = radio_driver.getPacketsSent();
    stats.total_air_time_secs = getTotalAirTime() / 1000;
    stats.total_up_time_secs = _ms->getMillis() / 1000;
    stats.n_sent_flood = getNumSentFlood();
    stats.n_sent_direct = getNumSentDirect();
    stats.n_recv_flood = getNumRecvFlood();
    stats.n_recv_direct = getNumRecvDirect();
    stats.err_events = _err_flags;
    stats.last_snr = radio_driver.getLastSNR() * 4;
    stats.n_direct_dups = ((SimpleMeshTables*)getTables())->getNumDirectDups();
    stats.n_flood_dups = ((SimpleMeshTables*)getTables())->getNumFloodDups();
    stats.total_rx_air_time_secs = getReceiveAirTime() / 1000;
    stats.n_recv_errors = radio_driver.getPacketsRecvErrors();
    if (capacity < sizeof(stats)) return 0;
    memcpy(reply, &stats, sizeof(stats));
    return sizeof(stats);
  }
  void onTxStarted(mesh::Packet* packet, uint32_t time) override { _bot.onTxStarted(packet, time); }
  void logRx(mesh::Packet* packet, int, float) override { _bot.observeRoute(packet); }
  void logTx(mesh::Packet* packet, int length) override { _bot.logTx(packet, length); }
  void logTxFail(mesh::Packet* packet, int length) override { _bot.logTxFail(packet, length); }
  void onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id, uint32_t time,
                     const uint8_t* data, size_t length) override {
    _bot.onAdvertRecv(packet, id, time, data, length);
  }
  void onTraceRecv(mesh::Packet* packet, uint32_t tag, uint32_t auth, uint8_t flags,
                   const uint8_t* snrs, const uint8_t* hashes, uint8_t length) override {
    _bot.onTraceRecv(packet, tag, auth, flags, snrs, hashes, length);
  }
  int searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int count) override {
    return _bot.searchChannelsByHash(hash, channels, count);
  }
  void onGroupDataRecv(mesh::Packet* packet, uint8_t type, const mesh::GroupChannel& channel,
                       uint8_t* data, size_t length) override {
    _bot.onGroupDataRecv(packet, type, channel, data, length);
  }

private:
  uint32_t now() override { return _ms->getMillis(); }
  uint32_t uniqueTime() override { return getRTCClock()->getCurrentTimeUnique(); }
  const char* nodeName() override { return getNodeName(); }
  mesh::RNG& rng() override { return *getRNG(); }
  uint32_t estimateAirtime(uint8_t length) override { return _radio->getEstAirtimeFor(length); }
  mesh::Packet* createTrace(uint32_t tag, uint32_t auth, uint8_t flags) override {
    return Mesh::createTrace(tag, auth, flags);
  }
  mesh::Packet* createGroupDatagram(uint8_t type, const mesh::GroupChannel& channel,
                                   const uint8_t* data, size_t length) override {
    return Mesh::createGroupDatagram(type, channel, data, length);
  }
  void sendFlood(mesh::Packet* packet, uint32_t delay) override { Mesh::sendFlood(packet, delay); }
  void sendDirect(mesh::Packet* packet, const uint8_t* path, uint8_t length,
                   uint32_t delay) override { Mesh::sendDirect(packet, path, length, delay); }
  void releasePacket(mesh::Packet* packet) override { Mesh::releasePacket(packet); }
  bool isQueuedPacket(const mesh::Packet* packet) const override { return Mesh::isQueuedPacket(packet); }
  bool cancelQueuedPacket(mesh::Packet* packet) override { return Mesh::cancelQueuedPacket(packet); }
};
