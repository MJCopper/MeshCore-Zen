#pragma once

#include <stdint.h>

// Pure timing policy. Radio airtime is supplied by the application adapter.
class TraceTimingPolicy {
public:
  static const uint32_t QUEUE_TIMEOUT_MILLIS = 10000;
  static const uint32_t FINAL_GRACE_MILLIS = 3000;
  static const uint32_t REPORT_TIMEOUT_MILLIS = 10000;

  static uint32_t attemptTimeout(uint8_t repeaters, uint32_t airtime_millis) {
    uint64_t minimum = 5000 + (uint64_t)repeaters * 2500;
    uint64_t estimated = 500 + ((uint64_t)airtime_millis * 6 + 250) * (2 * repeaters);
    uint64_t timeout = minimum > estimated ? minimum : estimated;
    timeout = ((timeout + 999) / 1000) * 1000;
    // Keep timer intervals within the wrap-safe half of the millisecond range.
    return timeout > 0x7FFFF000 ? 0x7FFFF000 : (uint32_t)timeout;
  }
};
