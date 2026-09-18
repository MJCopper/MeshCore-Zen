#pragma once

#include "../NodePrefs.h"
#include <stdint.h>

namespace solo {

// Tracks the last successfully persisted preference state. This avoids an
// unchanged shutdown (or repeated save request) rewriting the flash file.
class PrefsSaveTracker {
  uint64_t _saved = 0;
  bool _valid = false;

  static uint64_t addBytes(uint64_t hash, const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; i++) {
      hash ^= bytes[i];
      hash *= 1099511628211ULL;
    }
    return hash;
  }

  static uint64_t fingerprint(const NodePrefs& prefs, double lat, double lon) {
    uint64_t hash = addBytes(14695981039346656037ULL, &prefs, sizeof(prefs));
    hash = addBytes(hash, &lat, sizeof(lat));
    return addBytes(hash, &lon, sizeof(lon));
  }

public:
  bool needsSave(const NodePrefs& prefs, double lat, double lon) const {
    return !_valid || _saved != fingerprint(prefs, lat, lon);
  }

  void markSaved(const NodePrefs& prefs, double lat, double lon) {
    _saved = fingerprint(prefs, lat, lon);
    _valid = true;
  }
};

} // namespace solo
