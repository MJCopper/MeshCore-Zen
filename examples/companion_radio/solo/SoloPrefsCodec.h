#pragma once

#include "../NodePrefs.h"
#include "SoloFeatures.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Versioned Solo-only preference sidecar. The legacy fields remain mirrored in
// /new_prefs for one compatibility cycle; this explicit record format is the
// forward path that lets upstream NodePrefs evolve without shifting Solo data.
namespace solo {

class PrefsCodec {
  static constexpr uint32_t MAGIC = 0x46504C53UL; // "SLPF" little-endian
  static constexpr uint8_t VERSION = 1;
  // Versions 1..15 share this length-delimited record envelope. New optional
  // records can increment the version and remain readable by this decoder;
  // version 16 starts the next, deliberately incompatible envelope generation.
  static constexpr uint8_t MAX_COMPAT_VERSION = 15;
  static constexpr uint8_t REC_CHILD_MODE = 1;
  static constexpr uint8_t REC_QUIET_TIME = 2;

  static uint32_t checksum(const uint8_t* data, size_t len) {
    uint32_t h = 2166136261UL;
    for (size_t i = 0; i < len; i++) { h ^= data[i]; h *= 16777619UL; }
    return h;
  }

  static void put16(uint8_t*& p, uint16_t v) { *p++ = (uint8_t)v; *p++ = (uint8_t)(v >> 8); }
  static void put32(uint8_t*& p, uint32_t v) {
    for (int i = 0; i < 4; i++) { *p++ = (uint8_t)v; v >>= 8; }
  }
  static uint16_t get16(const uint8_t*& p) {
    uint16_t v = p[0] | ((uint16_t)p[1] << 8); p += 2; return v;
  }
  static uint32_t get32(const uint8_t*& p) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) v |= ((uint32_t)*p++) << (8 * i);
    return v;
  }

public:
  // Bounded for the nRF52 stack while leaving ample room for future optional
  // records. Unknown records are skipped by length during decode.
  static constexpr size_t MAX_ENCODED_SIZE = 128;

  static size_t encode(const NodePrefs& prefs, uint8_t* out, size_t capacity) {
    if (!out || capacity < MAX_ENCODED_SIZE) return 0;
    uint8_t* p = out;
    put32(p, MAGIC);
    *p++ = VERSION;
    uint8_t* payload_len = p; p += 2;
    uint8_t* payload_start = p;

    if (Features::CHILD_MODE) {
      *p++ = REC_CHILD_MODE; put16(p, 9);
      *p++ = prefs.child_mode_enabled;
      put32(p, prefs.child_mode_pin_hash);
      put16(p, prefs.child_visible_pages);
      *p++ = prefs.child_channels_enabled;
      *p++ = prefs.child_rooms_enabled;
    }

    if (Features::QUIET_TIME) {
      *p++ = REC_QUIET_TIME; put16(p, 5);
      *p++ = prefs.quiet_time_enabled;
      put16(p, prefs.quiet_time_start_min);
      put16(p, prefs.quiet_time_end_min);
    }

    uint16_t len = (uint16_t)(p - payload_start);
    payload_len[0] = (uint8_t)len;
    payload_len[1] = (uint8_t)(len >> 8);
    put32(p, checksum(out, (size_t)(p - out)));
    return (size_t)(p - out);
  }

  static bool decode(NodePrefs& prefs, const uint8_t* data, size_t size) {
    if (!data || size < 11) return false;
    const uint8_t* p = data;
    if (get32(p) != MAGIC) return false;
    uint8_t version = *p++;
    if (version < VERSION || version > MAX_COMPAT_VERSION) return false;
    uint16_t payload_len = get16(p);
    if ((size_t)payload_len + 11 != size) return false;
    const uint8_t* payload_end = p + payload_len;
    const uint8_t* checksum_pos = payload_end;
    const uint8_t* check_reader = checksum_pos;
    if (get32(check_reader) != checksum(data, (size_t)(checksum_pos - data))) return false;

    // Keep only the small Solo-owned values on the stack. NodePrefs is several
    // kilobytes on this target, so copying the whole structure here would waste
    // scarce nRF52 task stack merely to provide atomic decode semantics.
    uint8_t child_enabled = prefs.child_mode_enabled;
    uint32_t child_pin_hash = prefs.child_mode_pin_hash;
    uint16_t child_visible_pages = prefs.child_visible_pages;
    uint8_t child_channels_enabled = prefs.child_channels_enabled;
    uint8_t child_rooms_enabled = prefs.child_rooms_enabled;
    uint8_t quiet_enabled = prefs.quiet_time_enabled;
    uint16_t quiet_start_min = prefs.quiet_time_start_min;
    uint16_t quiet_end_min = prefs.quiet_time_end_min;
    while (p < payload_end) {
      if ((size_t)(payload_end - p) < 3) return false;
      uint8_t id = *p++;
      uint16_t len = get16(p);
      if ((size_t)(payload_end - p) < len) return false;
      const uint8_t* rec_end = p + len;
      if (Features::CHILD_MODE && id == REC_CHILD_MODE && (len == 8 || len == 9)) {
        child_enabled = *p++;
        child_pin_hash = get32(p);
        child_visible_pages = get16(p);
        child_channels_enabled = *p++;
        if (len == 9) child_rooms_enabled = *p++;
      } else if (Features::QUIET_TIME && id == REC_QUIET_TIME && len == 5) {
        quiet_enabled = *p++;
        quiet_start_min = get16(p);
        quiet_end_min = get16(p);
      }
      p = rec_end; // unknown records are intentionally skipped
    }

    if ((Features::CHILD_MODE && (child_enabled > 1 || child_channels_enabled > 1 || child_rooms_enabled > 1)) ||
        (Features::QUIET_TIME && (quiet_enabled > 1 || quiet_start_min >= 1440 ||
                                  quiet_end_min >= 1440))) return false;

    // Apply only after the complete file, checksum, records and values validate.
    if (Features::CHILD_MODE) {
      prefs.child_mode_enabled = child_enabled;
      prefs.child_mode_pin_hash = child_pin_hash;
      prefs.child_visible_pages = child_visible_pages;
      prefs.child_channels_enabled = child_channels_enabled;
      prefs.child_rooms_enabled = child_rooms_enabled;
    }
    if (Features::QUIET_TIME) {
      prefs.quiet_time_enabled = quiet_enabled;
      prefs.quiet_time_start_min = quiet_start_min;
      prefs.quiet_time_end_min = quiet_end_min;
    }
    return true;
  }
};

} // namespace solo
