#pragma once

#include <stdint.h>
#include "../../ui-new/QuietTimePolicy.h"

namespace zen { namespace pet {

// Pet-only schedule and RAM wake override; notification policy is independent.
class PetSleep {
  bool _wake = false;
  uint32_t _wake_at = 0;
public:
  static bool scheduled(bool synced, int64_t local, uint16_t start, uint16_t end) {
    if (!synced) return false;
    int64_t seconds = local % 86400;
    if (seconds < 0) seconds += 86400;
    return quiettime::intervalActive(seconds / 60,start,end);
  }
  bool sleeping(uint32_t now, bool enabled, bool schedule) {
    if (!enabled || !schedule || (_wake && uint32_t(now-_wake_at) >= 1800000UL)) _wake = false;
    return enabled && schedule && !_wake;
  }
  void wake(uint32_t now) { _wake = true; _wake_at = now; }
};

} }
