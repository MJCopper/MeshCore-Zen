#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include <helpers/IdentityStore.h>

// Repeater advert names used only to explain incoming Public-channel paths.
// The cache is separate from the sensor ACL and never saves unchanged adverts.
class RepeaterNameCache {
  static const uint8_t MAX_REPEATERS = 48;
  static const uint32_t SAVE_DELAY_MILLIS = 30000;
  static const uint32_t MIN_SAVE_INTERVAL_MILLIS = 600000;

  struct Entry {
    uint8_t pub_key[PUB_KEY_SIZE];
    char name[32];
    uint32_t last_advert_timestamp;
  };

  FILESYSTEM* _fs;
  Entry _entries[MAX_REPEATERS];
  uint8_t _count;
  uint8_t _next_replace;
  uint32_t _dirty_at;
  uint32_t _last_save_at;
  bool _dirty;
  bool _has_saved;

  const char* findName(const uint8_t* hash, uint8_t hash_size) const;
  bool load();
  bool save();

public:
  RepeaterNameCache();

  void begin(FILESYSTEM* fs);
  void loop(uint32_t now_millis);
  void onAdvert(const mesh::Identity& id, uint32_t timestamp, const uint8_t* app_data,
                size_t app_data_len, uint32_t now_millis);
  void formatPath(const mesh::Packet* packet, char* dest, size_t size) const;
};
