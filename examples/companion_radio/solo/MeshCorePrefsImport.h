#pragma once

#include "../NodePrefs.h"
#include <helpers/ConfigSerializer.h>
#include <math.h>
#include <string.h>

namespace solo {

// Read the upstream companion's /prefs.json without making its storage format
// authoritative for Zen. Keep this adapter separate from Zen's binary codec.
class MeshCorePrefsImport : public ConfigSerializer {
  class Radio : public ConfigSerializer {
    MeshCorePrefsImport& parent;
  protected:
    void structure() override {
      def("freq", parent.freq);
      def("bw", parent.bw);
      def("sf", parent.sf);
      def("cr", parent.cr);
      def("rxgain", parent.rx_gain);
      def("tx", parent.tx_power);
      def("af", parent.airtime_factor);
      def("rxdelay", parent.rx_delay);
      def("hash_mode", parent.path_hash_mode);
      def("multi_ack", parent.multi_acks);
    }
  public:
    explicit Radio(MeshCorePrefsImport& p) : parent(p) { }
  } radio;

  class GPS : public ConfigSerializer {
    MeshCorePrefsImport& parent;
  protected:
    void structure() override {
      def("en", parent.gps_enabled);
      def("int", parent.gps_interval);
      def("adv_loc", parent.advert_loc_policy);
    }
  public:
    explicit GPS(MeshCorePrefsImport& p) : parent(p) { }
  } gps;

  class Repeat : public ConfigSerializer {
    MeshCorePrefsImport& parent;
  protected:
    void structure() override { def("disable", parent.repeat_disabled); }
  public:
    explicit Repeat(MeshCorePrefsImport& p) : parent(p) { }
  } repeat;

  class Companion : public ConfigSerializer {
    MeshCorePrefsImport& parent;
  protected:
    void structure() override {
      def("auto_max", parent.autoadd_max_hops);
      def("defs_nm", parent.default_scope_name, sizeof(parent.default_scope_name));
      def("defs_key", parent.default_scope_key, sizeof(parent.default_scope_key));
      def("pin", parent.ble_pin);
      def("buzz_q", parent.buzzer_quiet);
      def("auto_add", parent.autoadd_config);
      def("man_add", parent.manual_add_contacts);
      def("tel_base", parent.telemetry_mode_base);
      def("tel_loc", parent.telemetry_mode_loc);
      def("tel_env", parent.telemetry_mode_env);
    }
  public:
    explicit Companion(MeshCorePrefsImport& p) : parent(p) { }
  } companion;

  char name[32] = {};
  double latitude = 0, longitude = 0;
  float freq = -1, bw = -1, airtime_factor = 0, rx_delay = 0;
  uint8_t sf = 0, cr = 0, rx_gain = 0, path_hash_mode = 0, multi_acks = 0;
  int8_t tx_power = 0;
  uint8_t gps_enabled = 0, advert_loc_policy = 0, repeat_disabled = 1;
  uint32_t gps_interval = 0, ble_pin = 0;
  uint8_t autoadd_max_hops = 0, buzzer_quiet = 0, autoadd_config = 0;
  uint8_t manual_add_contacts = 0, telemetry_mode_base = 0;
  uint8_t telemetry_mode_loc = 0, telemetry_mode_env = 0;
  char default_scope_name[31] = {};
  uint8_t default_scope_key[16] = {};

protected:
  void structure() override {
    def("name", name, sizeof(name));
    def("lat", latitude);
    def("lon", longitude);
    def("radio", radio);
    def("gps", gps);
    def("repeat", repeat);
    def("comp", companion);
  }

public:
  MeshCorePrefsImport() : radio(*this), gps(*this), repeat(*this), companion(*this) { }

  bool read(Stream& file) {
    if (!loadSerial(file)) return false;
    // A syntactically valid but incomplete record must not replace a working
    // radio configuration. These fields are present in every upstream save.
    return isfinite(freq) && freq >= 150 && freq <= 2500 &&
           isfinite(bw) && bw >= 7.8f && bw <= 500 &&
           sf >= 5 && sf <= 12 && cr >= 5 && cr <= 8 &&
           isfinite(latitude) && latitude >= -90 && latitude <= 90 &&
           isfinite(longitude) && longitude >= -180 && longitude <= 180 &&
           isfinite(airtime_factor) && isfinite(rx_delay);
  }

  void apply(NodePrefs& prefs, double& node_lat, double& node_lon) const {
    memcpy(prefs.node_name, name, sizeof(name));
    node_lat = latitude;
    node_lon = longitude;
    prefs.freq = freq;
    prefs.bw = bw;
    prefs.sf = sf;
    prefs.cr = cr;
    prefs.rx_boosted_gain = rx_gain;
    prefs.tx_power_dbm = tx_power;
    prefs.airtime_factor = airtime_factor;
    prefs.rx_delay_base = rx_delay;
    prefs.path_hash_mode = path_hash_mode;
    prefs.multi_acks = multi_acks;
    prefs.gps_enabled = gps_enabled;
    prefs.gps_interval = gps_interval;
    prefs.advert_loc_policy = advert_loc_policy;
    prefs.client_repeat = repeat_disabled ? 0 : 1;
    prefs.autoadd_max_hops = autoadd_max_hops;
    memcpy(prefs.default_scope_name, default_scope_name, sizeof(default_scope_name));
    memcpy(prefs.default_scope_key, default_scope_key, sizeof(default_scope_key));
    prefs.ble_pin = ble_pin;
    prefs.buzzer_quiet = buzzer_quiet;
    prefs.autoadd_config = autoadd_config;
    prefs.manual_add_contacts = manual_add_contacts;
    prefs.telemetry_mode_base = telemetry_mode_base;
    prefs.telemetry_mode_loc = telemetry_mode_loc;
    prefs.telemetry_mode_env = telemetry_mode_env;
  }
};

} // namespace solo
