#pragma once
#include <stdint.h>

// Stateless timing and quality rules for periodic GPS acquisition. Keeping
// these calculations outside the sensor loop makes the power policy testable.
class GpsPollingPolicy {
public:
  static constexpr long MAX_HDOP_TENTHS = 40;
  static constexpr long MIN_SATELLITES = 8;
  static constexpr uint32_t STATIONARY_STABLE_MS = 4000UL;
  static constexpr uint32_t MOVING_CAPTURE_MS = 25000UL;
  static constexpr uint32_t MAX_RETRY_DELAY_SEC = 6UL * 60UL * 60UL;

  static bool qualityGood(long hdop, long satellites) {
    return hdop >= 0 ? hdop <= MAX_HDOP_TENTHS : satellites >= MIN_SATELLITES;
  }
  static uint32_t retryDelaySeconds(uint32_t configured, uint8_t failures) {
    uint8_t shift = failures > 1 ? (failures > 2 ? 2 : 1) : 0;
    uint32_t delay = configured > (MAX_RETRY_DELAY_SEC >> shift)
                         ? MAX_RETRY_DELAY_SEC : configured << shift;
    return delay > MAX_RETRY_DELAY_SEC ? MAX_RETRY_DELAY_SEC : delay;
  }
};
