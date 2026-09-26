#pragma once

#include <cstdint>

namespace zen {

// Fixed radio timing for the companion's optional repeater backend. These
// values match the standard repeater defaults and are deliberately not prefs.
struct RepeaterTiming {
  static constexpr float RX_DELAY_BASE = 10.0f;
  static constexpr float FLOOD_TX_FACTOR = 0.5f;
  static constexpr float DIRECT_TX_FACTOR = 0.3f;
  static constexpr uint8_t YIELD_MULTIPLIER = 2;

  static uint32_t delayWindow(uint32_t airtime, float factor) {
    return static_cast<uint32_t>(airtime * factor);
  }
};

} // namespace zen
