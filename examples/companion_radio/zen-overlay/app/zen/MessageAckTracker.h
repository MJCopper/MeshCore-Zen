#pragma once

#include <stdint.h>

namespace zen {

// MeshCore ACKs hash the low two attempt bits. Four slots therefore cover all
// attempts of the same timestamp/text, including subsequent manual resends.
class MessageAckTracker {
  uint32_t _tags[4]{};
  uint8_t _routes[4]{};
public:
  void record(uint8_t attempt, uint32_t tag, uint8_t route) {
    _tags[attempt & 3] = tag;
    _routes[attempt & 3] = route;
  }
  bool match(uint32_t tag, uint8_t& route) const {
    if (!tag) return false;
    for (unsigned i = 0; i < 4; i++) {
      if (_tags[i] != tag) continue;
      route = _routes[i];
      return true;
    }
    return false;
  }
};

} // namespace zen
