#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace zen {

struct PathShape {
  bool valid;
  bool known;
  uint8_t hops;
  uint8_t hash_bytes;
};

// MeshCore packs hash width in the upper two bits and hop count in the lower
// six. 0xFF is the contact's "no learned path" sentinel, not a 63-hop route.
inline PathShape pathShape(uint8_t encoded, size_t capacity) {
  if (encoded == 0xFF) return {true, false, 0, 0};
  uint8_t width = (encoded >> 6) + 1;
  uint8_t hops = encoded & 63;
  bool valid = width <= 3 && (size_t)width * hops <= capacity;
  return {valid, valid, hops, width};
}

// A short hop hash is not an identity. Use a name only for exactly one saved
// public-key prefix; collisions and unknown hashes remain hexadecimal.
inline int uniquePrefixMatch(const uint8_t* hop, size_t width,
                             const uint8_t* keys, size_t count, size_t stride) {
  if (!hop || !keys || width < 1 || width > 3 || stride < width) return -1;
  int found = -1;
  for (size_t i = 0; i < count; i++) {
    if (memcmp(hop, keys + i * stride, width)) continue;
    if (found >= 0) return -1;
    found = (int)i;
  }
  return found;
}

struct PathAttemptSnapshot {
  uint8_t route = 0;
  uint8_t tries = 0;
  uint8_t hops = 0;
  uint8_t hash_bytes = 0;
  uint8_t result = 0;
  bool fallback_from_path = false;
  uint8_t fallback_hops = 0;
};

} // namespace zen
