#pragma once

#include <stdint.h>

namespace zen {

// Tracks received fixes rather than repeated reads of the parser's cache.
class GpsFreshness {
  uint32_t _sequence = 0;
  uint32_t _received_ms = 0;
  bool _valid = false;
public:
  void reset() { _valid = false; }
  void record(uint32_t now, bool valid) {
    _valid = valid;
    if (valid) { _received_ms = now; ++_sequence; }
  }
  bool valid(uint32_t now) const {
    return _valid && (uint32_t)(now - _received_ms) < 5000UL;
  }
  uint32_t sequence() const { return _sequence; }
  uint32_t receivedMs() const { return _received_ms; }
};

} // namespace zen
