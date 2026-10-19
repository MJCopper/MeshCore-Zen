#pragma once

#include <helpers/ClientACL.h>
#include <stddef.h>

struct SensorStats {
  uint16_t batt_milli_volts;
  uint16_t curr_tx_queue_len;
  int16_t noise_floor;
  int16_t last_rssi;
  uint32_t n_packets_recv;
  uint32_t n_packets_sent;
  uint32_t total_air_time_secs;
  uint32_t total_up_time_secs;
  uint32_t n_sent_flood, n_sent_direct;
  uint32_t n_recv_flood, n_recv_direct;
  uint16_t err_events;
  int16_t last_snr;  // x 4
  uint16_t n_direct_dups, n_flood_dups;
  uint32_t total_rx_air_time_secs;
  uint32_t n_recv_errors;
};

static_assert(sizeof(SensorStats) == 56, "Sensor status wire layout changed");

// Custom access and protocol policy; the base sensor owns encryption and routing.
class SensorManagementPolicy {
public:
  static ClientInfo* login(ClientACL& clients, const mesh::Identity& sender,
                           const uint8_t* secret, uint32_t timestamp, uint32_t now,
                           const char* password, const char* admin_password, bool& persist);
  static bool persistClient(ClientInfo* client) { return client->isAdmin(); }
  static uint8_t loginMode(const ClientInfo& client) { return client.isAdmin() ? 1 : 2; }
  static int ownerInfo(const char* version, const char* name, const char* owner,
                       uint8_t* reply, size_t capacity);
};

