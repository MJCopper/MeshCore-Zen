#pragma once

#include <stdint.h>

namespace zen {

class NotificationPopupState {
  uint8_t _priority = 0;

public:
  bool accept(uint8_t priority, uint32_t now, uint32_t current_expiry) {
    if ((int32_t)(current_expiry - now) > 0 && priority < _priority) return false;
    _priority = priority;
    return true;
  }
  void clear() { _priority = 0; }
  uint8_t priority() const { return _priority; }
};

} // namespace zen
