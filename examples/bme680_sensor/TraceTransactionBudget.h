#pragma once
#include <stdint.h>

// One user-visible limit, independent of route length and radio profile.
struct TraceTransactionBudget {
  static const uint32_t DURATION_MILLIS = 60000;
  static const uint32_t REPORT_RESERVE_MILLIS = 5000;
  static uint32_t seconds() { return DURATION_MILLIS / 1000; }
  static uint32_t remaining(uint32_t now, uint32_t deadline) {
    return (int32_t)(deadline - now) > 0 ? deadline - now : 0;
  }
  static uint32_t workRemaining(uint32_t now, uint32_t deadline) {
    uint32_t left = remaining(now, deadline);
    return left > REPORT_RESERVE_MILLIS ? left - REPORT_RESERVE_MILLIS : 0;
  }
  // Share the live work budget: initially two discovery and two probe attempts;
  // after discovery only the remaining probe attempts compete for that time.
  static uint32_t attemptWindow(uint32_t now, uint32_t deadline, uint8_t slots) {
    return slots ? workRemaining(now, deadline) / slots : 0;
  }
};
