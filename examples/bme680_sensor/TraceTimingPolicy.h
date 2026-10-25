#pragma once

#include <stdint.h>

// Pure timing policy. Radio airtime is supplied by the application adapter.
class TraceTimingPolicy {
public:
  static const uint32_t QUEUE_TIMEOUT_MILLIS = 10000;
  static const uint8_t LOGIN_MAX_ATTEMPTS = 3;
  static const uint32_t CONTROL_TIMEOUT_MILLIS = 40000;
  static const uint32_t LOGIN_QUEUE_TIMEOUT_MILLIS = 8000;
  static const uint32_t LOGIN_REPLY_TIMEOUT_MILLIS = 8000;
  static const uint32_t FINAL_GRACE_MILLIS = 3000;
  static const uint32_t REPORT_TIMEOUT_MILLIS = 10000;

  static uint32_t localBackoff(uint8_t failures) {
    return failures == 0 ? 250 : failures == 1 ? 500 : 1000;
  }
  static uint32_t settleDelay(uint32_t airtime, uint32_t jitter) {
    uint64_t delay = (uint64_t)airtime * 2;
    return (delay < 300 ? 300 : delay > 2000 ? 2000 : delay) + jitter % 251;
  }
  static uint32_t quietInterval(uint32_t airtime) {
    return airtime < 150 ? 150 : airtime > 750 ? 750 : airtime;
  }
  static uint32_t transmitWatchdog(uint32_t airtime) {
    uint64_t budget = (uint64_t)airtime * 3 / 2 + 1000;
    return budget > 0x7FFFF000 ? 0x7FFFF000 : (uint32_t)budget;
  }

  static uint32_t reportBudget(uint32_t airtime) {
    uint64_t budget = 26000 + (uint64_t)airtime * 3;
    return budget < 30000 ? 30000 : budget > 0x7FFFF000 ? 0x7FFFF000 : budget;
  }

  static uint32_t bounded(uint64_t value) { return value>0x7FFFF000?0x7FFFF000:(uint32_t)value; }
  static uint32_t floodTimeout(uint32_t airtime) { return bounded(1500ULL+16ULL*airtime); }
  static uint32_t directTimeout(uint32_t airtime,uint8_t entries) {
    return bounded(1500ULL+(6ULL*airtime+250)*(uint16_t(entries)+1));
  }
  static uint32_t attemptTimeout(uint8_t repeaters, uint32_t airtime_millis) {
    return directTimeout(airtime_millis,repeaters?2*repeaters-1:0);
  }
};
