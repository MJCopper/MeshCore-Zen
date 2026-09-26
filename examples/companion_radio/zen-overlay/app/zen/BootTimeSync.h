#pragma once

#include <cstdint>

namespace zen {

// Source-agnostic boot time-sync state. Hardware control stays in UITask; this
// class only schedules temporary GPS claims and decides when to release them.
class BootTimeSync {
public:
  enum class Action : uint8_t { NONE, START_TEMP_GPS, STOP_TEMP_GPS };
  static constexpr uint32_t GPS_TIMEOUT_MS = 5UL * 60UL * 1000UL;
  static constexpr uint32_t GPS_RETRY_TIMEOUT_MS = 90UL * 1000UL;
  static constexpr uint32_t RETRY_INTERVAL_MS = 60UL * 60UL * 1000UL;
  static constexpr uint32_t RETRY_WINDOW_MS = 48UL * 60UL * 60UL * 1000UL;

private:
  bool _pending = false;
  bool _retry_window_open = false;
  bool _has_gps = false;
  bool _owns_gps = false;
  uint32_t _generation = 0;
  uint32_t _deadline = 0;
  uint32_t _retry_at = 0;
  uint32_t _stop_at = 0;

public:
  void begin(uint32_t generation, bool has_gps, bool gps_configured_on, uint32_t now) {
    _pending = true;
    _retry_window_open = true;
    _has_gps = has_gps;
    _generation = generation;
    _owns_gps = has_gps && !gps_configured_on;
    _deadline = now + GPS_TIMEOUT_MS;
    _retry_at = now + RETRY_INTERVAL_MS;
    _stop_at = now + RETRY_WINDOW_MS;
  }

  bool pending() const { return _pending; }
  bool retryWindowOpen() const { return _retry_window_open; }
  bool shouldStartGps() const { return _owns_gps; }

  Action tick(uint32_t generation, bool gps_configured_on, bool gps_enabled,
              uint32_t now, bool allow_start = true) {
    if (!_pending) return Action::NONE;

    if (generation != _generation) {
      _pending = false;
      bool stop = _owns_gps && !gps_configured_on;
      _owns_gps = false;
      return stop ? Action::STOP_TEMP_GPS : Action::NONE;
    }

    // Stop automatic GPS retries after two days, but keep the sync pending
    // until an authoritative source actually updates the RTC. This leaves the
    // Clock screen on SYNC while allowing the receiver to remain powered down.
    if (_retry_window_open && (int32_t)(now - _stop_at) >= 0) {
      _retry_window_open = false;
      bool stop = _owns_gps && !gps_configured_on;
      _owns_gps = false;
      return stop ? Action::STOP_TEMP_GPS : Action::NONE;
    }

    // A manual enable takes ownership from the boot helper. A manual disable
    // has already stopped the receiver, so there is nothing left to release.
    if (_owns_gps && (gps_configured_on || !gps_enabled)) _owns_gps = false;

    if (_owns_gps && (int32_t)(now - _deadline) >= 0) {
      _owns_gps = false;
      return Action::STOP_TEMP_GPS;
    }

    // Still unsynchronised: once per hour, retry the same bounded temporary GPS
    // claim used at boot. GPS configured on is already the user's responsibility;
    // never toggle it behind their back.
    if (allow_start && _retry_window_open && _has_gps && !_owns_gps &&
        !gps_configured_on && !gps_enabled &&
        (int32_t)(now - _retry_at) >= 0) {
      _owns_gps = true;
      _deadline = now + GPS_RETRY_TIMEOUT_MS;
      _retry_at = now + RETRY_INTERVAL_MS;
      return Action::START_TEMP_GPS;
    }
    return Action::NONE;
  }
};

} // namespace zen
