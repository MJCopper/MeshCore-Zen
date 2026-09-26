#pragma once

#include <stdint.h>
#include <stddef.h>

namespace zen {

// A staged save needs room for the replacement while the original remains.
// Keep two filesystem blocks for LittleFS metadata and some growth headroom.
class StorageHealth {
public:
  // The current record must coexist with its replacement until commit.
  // This rejects only a definite shortfall; metadata and growth are handled
  // by checked writes, not by a speculative hard limit.
  static bool cannotStageReplacement(uint32_t free_bytes, uint32_t current_bytes) {
    return current_bytes && free_bytes < current_bytes;
  }

  static bool lowSpace(uint32_t free_bytes, uint32_t largest_file_bytes,
                       uint32_t block_bytes) {
    uint64_t required = (uint64_t)largest_file_bytes + 4096 +
                        (uint64_t)block_bytes * 2;
    return free_bytes < required;
  }
};

// Tracks a serialised record without allocating a second copy in RAM.
class CheckedRecordDigest {
  bool _good = true;
  size_t _size = 0;
  uint32_t _hash = 2166136261UL;

public:
  void record(const uint8_t* data, size_t requested, size_t written) {
    if (written != requested) { _good = false; return; }
    if (!_good) return;
    for (size_t i = 0; i < requested; i++) {
      _hash ^= data[i];
      _hash *= 16777619UL;
    }
    _size += requested;
  }
  bool good() const { return _good; }
  size_t size() const { return _size; }
  uint32_t hash() const { return _hash; }
};

} // namespace zen
