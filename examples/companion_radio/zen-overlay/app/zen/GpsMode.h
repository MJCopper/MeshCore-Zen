#pragma once

#include <stdint.h>

namespace zen {

// User-facing GPS power modes. The stored interval remains seconds so existing
// preferences and companion commands retain their wire/storage representation.
class GpsMode {
public:
  static constexpr uint8_t COUNT = 8;
  static constexpr uint8_t POLLING_COUNT = COUNT - 1;

  static uint32_t interval(uint8_t mode) {
    static const uint32_t VALUES[COUNT] = {
      0, 0, 0, 120, 300, 900, 1800, 3600
    };
    return VALUES[mode < COUNT ? mode : 0];
  }

  static const char* label(uint8_t mode) {
    static const char* LABELS[COUNT] = {
      "Off", "Continuous", "Adaptive", "2mins", "5mins", "15mins",
      "30mins", "1hr"
    };
    return LABELS[mode < COUNT ? mode : 0];
  }

  static uint8_t fromPrefs(bool enabled, uint32_t seconds, bool adaptive = false) {
    if (!enabled) return 0;
    if (adaptive && seconds == 0) return 2;
    if (seconds == 0) return 1;

    uint8_t nearest = 3;
    uint32_t nearest_delta = absDelta(seconds, interval(nearest));
    for (uint8_t mode = 4; mode < COUNT; mode++) {
      uint32_t delta = absDelta(seconds, interval(mode));
      if (delta < nearest_delta) {
        nearest = mode;
        nearest_delta = delta;
      }
    }
    return nearest;
  }

  // Polling choices deliberately exclude Off. GPS power is a separate setting;
  // retaining the cadence while GPS is off lets it resume unchanged later.
  static uint32_t pollingInterval(uint8_t choice) {
    return interval(choice < POLLING_COUNT ? choice + 1 : 1);
  }

  static const char* pollingLabel(uint8_t choice) {
    return label(choice < POLLING_COUNT ? choice + 1 : 1);
  }

  static uint8_t pollingFromInterval(uint32_t seconds) {
    return fromPrefs(true, seconds, false) - 1;
  }

  static uint8_t pollingFromPrefs(uint32_t seconds, bool adaptive) {
    return fromPrefs(true, seconds, adaptive) - 1;
  }

  static bool isAdaptive(uint8_t mode) {
    return mode == 2;
  }

  static bool pollingIsAdaptive(uint8_t choice) {
    return isAdaptive(choice < POLLING_COUNT ? choice + 1 : 1);
  }

private:
  static uint32_t absDelta(uint32_t a, uint32_t b) {
    return a > b ? a - b : b - a;
  }
};

} // namespace zen
