#pragma once

#include "../../../NodePrefs.h"
#include <stdint.h>
#include <string.h>

namespace zen {

// Change detection only. Serialization remains exclusively in MeshCore's
// NodePrefs::saveSerial() and DataStore::savePrefs().
class BaselinePrefsFingerprint {
  uint64_t _hash = 14695981039346656037ULL;
  void add(const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; i++) {
      _hash ^= bytes[i];
      _hash *= 1099511628211ULL;
    }
  }

public:
  static uint64_t calculate(const NodePrefs& p) {
    BaselinePrefsFingerprint f;
#define BASE_PREF(field) f.add(&p.field, sizeof(p.field))
    BASE_PREF(airtime_factor); f.add(p.node_name, sizeof(p.node_name));
    BASE_PREF(node_lat); BASE_PREF(node_lon); BASE_PREF(freq); BASE_PREF(sf);
    BASE_PREF(cr); BASE_PREF(multi_acks); BASE_PREF(manual_add_contacts);
    BASE_PREF(bw); BASE_PREF(tx_power_dbm); BASE_PREF(telemetry_mode_base);
    BASE_PREF(telemetry_mode_loc); BASE_PREF(telemetry_mode_env);
    BASE_PREF(rx_delay_base); BASE_PREF(ble_pin); BASE_PREF(advert_loc_policy);
    BASE_PREF(buzzer_quiet); BASE_PREF(vibe_quiet); BASE_PREF(gps_enabled);
    BASE_PREF(gps_interval); BASE_PREF(autoadd_config); BASE_PREF(rx_boosted_gain);
    BASE_PREF(radio_fem_rxgain); BASE_PREF(radio_fem_txgain);
    uint8_t repeat = p.isRepeatEn() ? 1 : 0; f.add(&repeat, sizeof(repeat));
    BASE_PREF(path_hash_mode); BASE_PREF(autoadd_max_hops);
    f.add(p.default_scope_name, sizeof(p.default_scope_name));
    f.add(p.default_scope_key, sizeof(p.default_scope_key));
#undef BASE_PREF
    return f._hash;
  }
};

} // namespace zen
