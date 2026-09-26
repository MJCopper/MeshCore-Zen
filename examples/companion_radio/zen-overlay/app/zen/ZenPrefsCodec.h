#pragma once

#include "../ZenPrefs.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace zen {

// Versioned, tagged Zen preference overlay. This deliberately contains only
// active Zen/UI settings; MeshCore-compatible radio and identity data remain
// in their established stores. Unknown records are skipped by length.
class ZenPrefsCodec {
  static constexpr uint32_t MAGIC = 0x46504E5AUL; // "ZNPF" little-endian
  static constexpr uint8_t VERSION = 2;
  static constexpr uint8_t MIN_COMPAT_VERSION = 1;
  // Older records are migrated explicitly. Never accept a newer envelope and
  // then rewrite it without the records that this codec does not understand.
  static constexpr uint8_t MAX_COMPAT_VERSION = VERSION;
  enum Record : uint8_t {
    REC_CHILD = 1,
    REC_QUIET = 2,
    REC_SYSTEM = 3,
    REC_UI = 4,
    REC_NOTIFICATIONS = 5,
    REC_QUICK_REPLIES = 6,
    REC_RADIO_PRESETS = 7,
    REC_ADVERT = 8,
    REC_ZEN_TAIL = 9,
  };

  class Writer {
    uint8_t* _p;
    uint8_t* _end;
    bool _good = true;
  public:
    Writer(uint8_t* out, size_t size) : _p(out), _end(out + size) {}
    bool put(const void* src, size_t size) {
      if (!_good || (size_t)(_end - _p) < size) return _good = false;
      memcpy(_p, src, size); _p += size; return true;
    }
    void u8(uint8_t value) { put(&value, 1); }
    void u16(uint16_t value) { uint8_t b[2] = {(uint8_t)value, (uint8_t)(value >> 8)}; put(b, 2); }
    void u32(uint32_t value) {
      uint8_t b[4] = {(uint8_t)value, (uint8_t)(value >> 8), (uint8_t)(value >> 16), (uint8_t)(value >> 24)};
      put(b, 4);
    }
    uint8_t* position() const { return _p; }
    bool good() const { return _good; }
  };

  class Reader {
    const uint8_t* _p;
    const uint8_t* _end;
    bool _good = true;
  public:
    Reader(const uint8_t* data, size_t size) : _p(data), _end(data + size) {}
    bool get(void* dst, size_t size) {
      if (!_good || (size_t)(_end - _p) < size) return _good = false;
      memcpy(dst, _p, size); _p += size; return true;
    }
    uint8_t u8() { uint8_t v = 0; get(&v, 1); return v; }
    uint16_t u16() { uint8_t b[2] = {}; get(b, 2); return (uint16_t)b[0] | ((uint16_t)b[1] << 8); }
    uint32_t u32() {
      uint8_t b[4] = {}; get(b, 4);
      return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    }
    const uint8_t* position() const { return _p; }
    size_t remaining() const { return (size_t)(_end - _p); }
    bool good() const { return _good; }
  };

  static uint32_t checksum(const uint8_t* data, size_t len) {
    uint32_t h = 2166136261UL;
    for (size_t i = 0; i < len; i++) { h ^= data[i]; h *= 16777619UL; }
    return h;
  }

  static void beginRecord(Writer& w, uint8_t id, uint16_t len) { w.u8(id); w.u16(len); }

  static bool inspectEnvelope(const uint8_t* data, size_t size, uint32_t* generation,
                              const uint8_t** payload, uint16_t* payload_len) {
    if (!data || size < 17) return false;
    Reader r(data, size);
    if (r.u32() != MAGIC) return false;
    uint8_t version = r.u8();
    if (version < MIN_COMPAT_VERSION || version > MAX_COMPAT_VERSION) return false;
    uint16_t len = r.u16();
    uint32_t gen = r.u32();
    (void)r.u16(); // semantic schema; individual records remain authoritative
    if (!r.good() || (size_t)len + 17 != size) return false;
    const uint8_t* start = r.position();
    const uint8_t* checksum_pos = start + len;
    Reader tail(checksum_pos, 4);
    if (tail.u32() != checksum(data, (size_t)(checksum_pos - data))) return false;

    // Validate the complete TLV structure before any preference is changed.
    Reader records(start, len);
    while (records.remaining()) {
      if (records.remaining() < 3) return false;
      (void)records.u8();
      uint16_t record_len = records.u16();
      if (!records.good() || records.remaining() < record_len) return false;
      uint8_t scratch[16];
      while (record_len) {
        uint16_t chunk = record_len > sizeof(scratch) ? sizeof(scratch) : record_len;
        records.get(scratch, chunk);
        record_len -= chunk;
      }
    }
    if (!records.good()) return false;
    if (generation) *generation = gen;
    if (payload) *payload = start;
    if (payload_len) *payload_len = len;
    return true;
  }

public:
  static constexpr size_t MAX_ENCODED_SIZE = 4096;

  static bool hasNewerVersion(const uint8_t* data, size_t size) {
    if (!data || size < 5) return false;
    Reader r(data, size);
    return r.u32() == MAGIC && r.u8() > VERSION;
  }

  static size_t encode(const ZenPrefs& p, uint32_t generation, uint8_t* out, size_t capacity) {
    if (!out || capacity < MAX_ENCODED_SIZE) return 0;
    Writer w(out, capacity);
    w.u32(MAGIC); w.u8(VERSION);
    uint8_t* len_pos = w.position(); w.u16(0);
    w.u32(generation); w.u16(p.zen_config_schema);
    uint8_t* payload = w.position();

    beginRecord(w, REC_CHILD, 9);
    w.u8(p.child_mode_enabled); w.u32(p.child_mode_pin_hash); w.u16(p.child_visible_pages);
    w.u8(p.child_channels_enabled); w.u8(p.child_rooms_enabled);

    beginRecord(w, REC_QUIET, 5);
    w.u8(p.quiet_time_enabled); w.u16(p.quiet_time_start_min); w.u16(p.quiet_time_end_min);

    beginRecord(w, REC_SYSTEM, 9);
    w.u8(p.bluetooth_enabled); w.u8(p.gps_adaptive); w.u8(p.timezone_mode);
    w.u8(p.timezone_city); w.put(&p.timezone_manual_min, sizeof(p.timezone_manual_min));
    w.u8(p.notification_screen_wake); w.u16(p.zen_config_schema);

    const uint16_t ui_len = 2 + 1 + ZenPrefs::PAGE_ORDER_LEN + 1 + 2 + 11 + 8 + 1 +
                            sizeof(p.favourite_contacts);
    beginRecord(w, REC_UI, ui_len);
    w.u16(p.home_pages_mask); w.u8(p.page_order_set); w.put(p.page_order, sizeof(p.page_order));
    w.u8(p.display_brightness); w.put(&p.auto_off_secs, sizeof(p.auto_off_secs));
    w.u8(p.clock_12h); w.u8(p.clock_hide_seconds); w.u8(p.use_lemon_font); w.u8(p.display_rotation);
    w.u8(p.joystick_rotation); w.u8(p.eink_full_refresh_every); w.u8(p.keyboard_type);
    w.u8(p.batt_display_mode); w.u8(p.units_imperial); w.u8(p.dm_show_all); w.u8(p.room_fav_only);
    w.put(&p.ch_fav_bitmask, sizeof(p.ch_fav_bitmask)); w.u8(p.ch_fav_only);
    w.put(p.favourite_contacts, sizeof(p.favourite_contacts));

    const uint16_t notif_len = 8 + 2 + sizeof(p.ringtone_notes) + 2 + sizeof(p.ringtone2_notes) +
                               sizeof(p.ch_notif_override) + sizeof(p.ch_notif_muted) +
                               sizeof(p.dm_notif) + sizeof(p.dm_melody) +
                               sizeof(p.channel_melody_overrides);
    beginRecord(w, REC_NOTIFICATIONS, notif_len);
    // Retain the byte position for backward-compatible decoding, but do not
    // duplicate MeshCore's buzzer_quiet preference in the Zen-owned record.
    w.u8(0); w.u8(p.buzzer_volume); w.u8(p.buzzer_auto);
    w.u8(p.notif_melody_dm); w.u8(p.notif_melody_ch); w.u8(p.notif_melody_ad);
    w.u8(p.notif_melody_new_contact); w.u8(p.advert_sound_scope);
    w.u8(p.ringtone_bpm_idx); w.u8(p.ringtone_len); w.put(p.ringtone_notes, sizeof(p.ringtone_notes));
    w.u8(p.ringtone2_bpm_idx); w.u8(p.ringtone2_len); w.put(p.ringtone2_notes, sizeof(p.ringtone2_notes));
    w.put(&p.ch_notif_override, sizeof(p.ch_notif_override));
    w.put(&p.ch_notif_muted, sizeof(p.ch_notif_muted));
    w.put(p.dm_notif, sizeof(p.dm_notif)); w.put(p.dm_melody, sizeof(p.dm_melody));
    w.put(p.channel_melody_overrides, sizeof(p.channel_melody_overrides));

    beginRecord(w, REC_QUICK_REPLIES, 5 * sizeof(p.custom_msgs[0]));
    w.put(p.custom_msgs, 5 * sizeof(p.custom_msgs[0]));

    const uint16_t preset_len = ZenPrefs::USER_RADIO_PRESET_MAX * (16 + 4 + 4 + 1 + 1);
    beginRecord(w, REC_RADIO_PRESETS, preset_len);
    for (uint8_t i = 0; i < ZenPrefs::USER_RADIO_PRESET_MAX; i++) {
      w.put(p.user_radio_presets[i].name, sizeof(p.user_radio_presets[i].name));
      w.put(&p.user_radio_presets[i].freq, sizeof(float));
      w.put(&p.user_radio_presets[i].bw, sizeof(float));
      w.u8(p.user_radio_presets[i].sf); w.u8(p.user_radio_presets[i].cr);
    }

    beginRecord(w, REC_ADVERT, 4);
    w.put(&p.advert_auto_interval_sec, sizeof(p.advert_auto_interval_sec));

    // Persist every field owned by Zen. NodePrefs remains outside this record,
    // so the complete Zen tail survives reboot without shadowing MeshCore data.
    const size_t zen_tail_offset = (size_t)(
      reinterpret_cast<const uint8_t*>(&p.buzzer_volume) -
      reinterpret_cast<const uint8_t*>(&p));
    const uint16_t zen_tail_len = (uint16_t)(sizeof(ZenPrefs) - zen_tail_offset);
    beginRecord(w, REC_ZEN_TAIL, zen_tail_len);
    w.put(reinterpret_cast<const uint8_t*>(&p) + zen_tail_offset, zen_tail_len);

    if (!w.good()) return 0;
    uint16_t payload_len = (uint16_t)(w.position() - payload);
    len_pos[0] = (uint8_t)payload_len; len_pos[1] = (uint8_t)(payload_len >> 8);
    uint32_t digest = checksum(out, (size_t)(w.position() - out));
    w.u32(digest);
    return w.good() ? (size_t)(w.position() - out) : 0;
  }

  static uint64_t fingerprint(const ZenPrefs& p, uint8_t* scratch,
                              size_t capacity) {
    size_t size = encode(p, 0, scratch, capacity);
    if (!size) return 0;
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < size; i++) {
      hash ^= scratch[i];
      hash *= 1099511628211ULL;
    }
    return hash;
  }

  static bool inspect(const uint8_t* data, size_t size, uint32_t& generation) {
    return inspectEnvelope(data, size, &generation, nullptr, nullptr);
  }

  static bool decode(ZenPrefs& p, const uint8_t* data, size_t size, uint32_t* generation = nullptr) {
    const uint8_t* payload = nullptr;
    uint16_t payload_len = 0;
    if (!inspectEnvelope(data, size, generation, &payload, &payload_len)) return false;
    Reader records(payload, payload_len);
    while (records.remaining()) {
      uint8_t id = records.u8();
      uint16_t len = records.u16();
      const uint8_t* body = records.position();
      Reader r(body, len);
      if (id == REC_CHILD && len == 9) {
        p.child_mode_enabled = r.u8(); p.child_mode_pin_hash = r.u32();
        p.child_visible_pages = r.u16(); p.child_channels_enabled = r.u8(); p.child_rooms_enabled = r.u8();
      } else if (id == REC_QUIET && len == 5) {
        p.quiet_time_enabled = r.u8(); p.quiet_time_start_min = r.u16(); p.quiet_time_end_min = r.u16();
      } else if (id == REC_SYSTEM && len == 9) {
        p.bluetooth_enabled = r.u8(); p.gps_adaptive = r.u8(); p.timezone_mode = r.u8();
        p.timezone_city = r.u8(); r.get(&p.timezone_manual_min, sizeof(p.timezone_manual_min));
        p.notification_screen_wake = r.u8(); p.zen_config_schema = r.u16();
      } else if (id == REC_UI) {
        const uint16_t expected = 2 + 1 + ZenPrefs::PAGE_ORDER_LEN + 1 + 2 + 11 + 8 + 1 +
                                  sizeof(p.favourite_contacts);
        const uint16_t legacy_expected = expected - 2;
        if (len == expected || len == legacy_expected) {
          p.home_pages_mask = r.u16(); p.page_order_set = r.u8(); r.get(p.page_order, sizeof(p.page_order));
          p.display_brightness = r.u8(); r.get(&p.auto_off_secs, sizeof(p.auto_off_secs));
          p.clock_12h = r.u8();
          if (len == expected) p.clock_hide_seconds = r.u8();
          p.use_lemon_font = r.u8();
          p.display_rotation = r.u8();
          p.joystick_rotation = r.u8(); p.eink_full_refresh_every = r.u8(); p.keyboard_type = r.u8();
          p.batt_display_mode = r.u8();
          if (len == expected) p.units_imperial = r.u8();
          p.dm_show_all = r.u8(); p.room_fav_only = r.u8();
          r.get(&p.ch_fav_bitmask, sizeof(p.ch_fav_bitmask)); p.ch_fav_only = r.u8();
          r.get(p.favourite_contacts, sizeof(p.favourite_contacts));
        }
      } else if (id == REC_NOTIFICATIONS) {
        const uint16_t expected = 8 + 2 + sizeof(p.ringtone_notes) + 2 + sizeof(p.ringtone2_notes) +
                                  sizeof(p.ch_notif_override) + sizeof(p.ch_notif_muted) +
                                  sizeof(p.dm_notif) + sizeof(p.dm_melody) + sizeof(p.channel_melody_overrides);
        const uint16_t legacy_expected = expected - 4 - sizeof(p.ringtone_notes) - sizeof(p.ringtone2_notes);
        if (len == expected || len == legacy_expected) {
          (void)r.u8(); // legacy duplicate; MeshCore /prefs.json is authoritative
          p.buzzer_volume = r.u8(); p.buzzer_auto = r.u8();
          p.notif_melody_dm = r.u8(); p.notif_melody_ch = r.u8(); p.notif_melody_ad = r.u8();
          p.notif_melody_new_contact = r.u8(); p.advert_sound_scope = r.u8();
          if (len == expected) {
            p.ringtone_bpm_idx = r.u8(); p.ringtone_len = r.u8();
            r.get(p.ringtone_notes, sizeof(p.ringtone_notes));
            p.ringtone2_bpm_idx = r.u8(); p.ringtone2_len = r.u8();
            r.get(p.ringtone2_notes, sizeof(p.ringtone2_notes));
          }
          r.get(&p.ch_notif_override, sizeof(p.ch_notif_override));
          r.get(&p.ch_notif_muted, sizeof(p.ch_notif_muted));
          r.get(p.dm_notif, sizeof(p.dm_notif)); r.get(p.dm_melody, sizeof(p.dm_melody));
          r.get(p.channel_melody_overrides, sizeof(p.channel_melody_overrides));
        }
      } else if (id == REC_QUICK_REPLIES && len == 5 * sizeof(p.custom_msgs[0])) {
        r.get(p.custom_msgs, 5 * sizeof(p.custom_msgs[0]));
      } else if (id == REC_RADIO_PRESETS &&
                 len == ZenPrefs::USER_RADIO_PRESET_MAX * (16 + 4 + 4 + 1 + 1)) {
        for (uint8_t i = 0; i < ZenPrefs::USER_RADIO_PRESET_MAX; i++) {
          r.get(p.user_radio_presets[i].name, sizeof(p.user_radio_presets[i].name));
          r.get(&p.user_radio_presets[i].freq, sizeof(float));
          r.get(&p.user_radio_presets[i].bw, sizeof(float));
          p.user_radio_presets[i].sf = r.u8(); p.user_radio_presets[i].cr = r.u8();
        }
      } else if (id == REC_ADVERT && len == 4) {
        r.get(&p.advert_auto_interval_sec, sizeof(p.advert_auto_interval_sec));
      } else if (id == REC_ZEN_TAIL) {
        const size_t zen_tail_offset = (size_t)(
            reinterpret_cast<const uint8_t*>(&p.buzzer_volume) -
            reinterpret_cast<const uint8_t*>(&p));
        if (len == sizeof(ZenPrefs) - zen_tail_offset)
          r.get(reinterpret_cast<uint8_t*>(&p) + zen_tail_offset, len);
      }
      // Advance over known or unknown record without trusting a handler's reads.
      uint8_t scratch[16];
      Reader skip(body, len);
      size_t remaining = len;
      while (remaining) {
        size_t chunk = remaining > sizeof(scratch) ? sizeof(scratch) : remaining;
        skip.get(scratch, chunk); remaining -= chunk;
      }
      records = Reader(body + len, records.remaining() - len);
    }
    return true;
  }
};

} // namespace zen
