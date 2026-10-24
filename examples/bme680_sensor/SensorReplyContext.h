#pragma once
#include <helpers/PacketScope.h>

// Captured at acceptance and copied with each delayed reply; no receive pointers.
struct SensorReplyContext {
  uint8_t scope_key[16]{};  // all zeroes means explicitly unscoped
  uint8_t channel = 0;
  uint8_t hash_size = 1;
  SensorReplyContext() = default;
  explicit SensorReplyContext(uint8_t index) : channel(index) { }
};
