#pragma once

#include <stdint.h>

namespace zen {

// Companion time is authoritative and may legitimately correct a fast RTC
// backwards. Bounds reject malformed epochs without imposing forward-only
// clock behaviour. Small corrections need not cause an immediate flash write;
// the normal shutdown path persists the latest RTC value.
class DeviceTimePolicy {
public:
  static constexpr uint32_t MIN_EPOCH = 1704067200UL; // 2024-01-01 UTC
  static constexpr uint32_t MAX_EPOCH = 4102444799UL; // 2099-12-31 UTC
  static constexpr uint32_t PERSIST_DELTA_SECONDS = 60UL;

  static bool valid(uint32_t epoch) {
    return epoch >= MIN_EPOCH && epoch <= MAX_EPOCH;
  }

  static bool shouldPersist(uint32_t previous, uint32_t replacement) {
    if (!valid(previous)) return true;
    uint32_t delta = previous > replacement ? previous - replacement
                                            : replacement - previous;
    return delta >= PERSIST_DELTA_SECONDS;
  }
};

} // namespace zen
