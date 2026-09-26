#pragma once

#include <stdint.h>

namespace zen {

// Fixed battery policy shared by the shutdown check and local indicator.
// Percentage is a voltage-based estimate, not a fuel-gauge measurement.
class BatteryPolicy {
public:
  static constexpr uint16_t SHUTDOWN_MV = 3300;
  static constexpr uint16_t EMPTY_MV = 3300;
  static constexpr uint16_t FULL_MV = 4120;

  static bool shouldShutdown(uint16_t mv, bool external_power) {
    return !external_power && mv > 0 && mv <= SHUTDOWN_MV;
  }

  // Hundredths retain enough resolution for a multi-hour discharge-rate
  // estimate while keeping the interpolation integer-only.
  static uint16_t percentX100(int mv) {
    static const struct { uint16_t mv; uint16_t pct_x100; } CURVE[] = {
      {EMPTY_MV, 0}, {3450, 300}, {3550, 700}, {3600, 1200},
      {3650, 2000}, {3700, 3000}, {3750, 4200}, {3800, 5500},
      {3900, 7200}, {4000, 8500}, {4050, 9100}, {4100, 9700},
      {FULL_MV, 10000}
    };
    if (mv <= EMPTY_MV) return 0;
    if (mv >= FULL_MV) return 10000;
    // Piecewise interpolation follows the broad shape of a lightly loaded
    // single-cell Li-ion/LiPo discharge. Round to the nearest percentage.
    for (unsigned i = 1; i < sizeof(CURVE) / sizeof(CURVE[0]); i++) {
      if (mv <= CURVE[i].mv) {
        int span = CURVE[i].mv - CURVE[i - 1].mv;
        int32_t rise = (int32_t)(mv - CURVE[i - 1].mv) *
                       (CURVE[i].pct_x100 - CURVE[i - 1].pct_x100);
        return CURVE[i - 1].pct_x100 + (rise + span / 2) / span;
      }
    }
    return 10000;
  }

  static int percent(int mv) {
    int result = (percentX100(mv) + 50) / 100;
    // Reserve 100% for a measured full-charge voltage.
    return mv < FULL_MV && result >= 100 ? 99 : result;
  }
};

// Checked alongside the existing battery sample, with no extra wake timer.
// Keep the last alert across brief voltage recovery to avoid threshold chatter.
class LowBatteryReminder {
  uint32_t _last_alert = 0;
  bool _notified = false;
public:
  static constexpr uint32_t INTERVAL_MS = 60UL * 60UL * 1000UL;
  bool due(uint32_t now, uint16_t mv, bool external_power) {
    if (external_power) { _notified = false; return false; }
    if (!mv || BatteryPolicy::shouldShutdown(mv, false) ||
        BatteryPolicy::percent(mv) > 20) return false;
    if (_notified && (uint32_t)(now - _last_alert) < INTERVAL_MS) return false;
    _last_alert = now;
    _notified = true;
    return true;
  }
};

class LowPowerLatch {
  bool _active = false;
  uint8_t _low_samples = 0;
public:
  static constexpr uint8_t ENTER_PERCENT = 5;
  static constexpr uint8_t REQUIRED_SAMPLES = 3;
  bool update(uint16_t mv, bool external_power) {
    if (_active) {
      if (external_power) { _active = false; _low_samples = 0; }
    } else if (mv && !external_power && !BatteryPolicy::shouldShutdown(mv, false) &&
               BatteryPolicy::percent(mv) <= ENTER_PERCENT) {
      if (++_low_samples >= REQUIRED_SAMPLES) _active = true;
    } else {
      _low_samples = 0;
    }
    return _active;
  }
  bool active() const { return _active; }
};

class EmergencyWindow {
  bool _active = false;
  uint32_t _deadline = 0;
public:
  static constexpr uint32_t DURATION_MS = 10UL * 60UL * 1000UL;
  void start(uint32_t now) { _active = true; _deadline = now + DURATION_MS; }
  void cancel() { _active = false; _deadline = 0; }
  bool update(uint32_t now) {
    if (_active && (int32_t)(now - _deadline) >= 0) cancel();
    return _active;
  }
  bool active() const { return _active; }
  uint32_t remainingSeconds(uint32_t now) const {
    if (!_active || (int32_t)(now - _deadline) >= 0) return 0;
    return ((uint32_t)(_deadline - now) + 999) / 1000;
  }
};

} // namespace zen
