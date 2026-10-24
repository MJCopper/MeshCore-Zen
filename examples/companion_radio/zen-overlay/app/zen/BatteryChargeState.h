#pragma once

#include <stdint.h>

namespace zen {
// Inferred from USB power and filtered voltage, not charger hardware status.
class BatteryChargeState {
public:
  enum State : uint8_t { BATTERY, EXTERNAL_UNKNOWN, CHARGING, COMPLETE };
  static constexpr uint16_t COMPLETE_MV = 4150;
  static constexpr uint16_t RESUME_MV = 4120;
private:
  State _state = BATTERY;
public:
  State state() const { return _state; }
  // Invalid readings drop the completion latch; recovery needs 4.15 V again.
  bool update(uint16_t mv, bool valid, bool external_power) {
    State next = BATTERY;
    if (external_power) {
      if (!valid || !mv) next = EXTERNAL_UNKNOWN;
      else if (mv >= COMPLETE_MV || (_state == COMPLETE && mv >= RESUME_MV))
        next = COMPLETE;
      else next = CHARGING;
    }
    bool changed = next != _state;
    _state = next;
    return changed;
  }
  static bool shouldRefresh(bool changed, bool display_on, bool home_visible) {
    return changed && display_on && home_visible;
  }
};
} // namespace zen
